#include "net/mqtt_connection.h"

#include <Arduino.h>

#include <cstdlib>
#include <cstring>

// One received message, passed from the MQTT task to tick(). The topic and
// payload share one allocation starting at `topic`, which tick() frees.
struct InboxMessage {
  char* topic;
  char* payload;
};

// Room for a burst of messages between two loop() passes. The biggest burst
// is the retained messages the broker sends right after we subscribe.
static constexpr int INBOX_LENGTH = 16;

void MqttConnection::subscribe(const char* topic) {
  if (subscriptionCount_ < MAX_SUBSCRIPTIONS) subscriptions_[subscriptionCount_++] = topic;
}

void MqttConnection::begin(const char* host, uint16_t port, const char* user, const char* password,
                           const char* name, const Options& options) {
  if (client_) return;  // already started

  bufferSize_ = options.bufferSize;

  inbox_ = xQueueCreate(INBOX_LENGTH, sizeof(InboxMessage));

  // The end of the MAC address keeps the client id unique if there's ever
  // more than one device with the same name.
  snprintf(clientId_, sizeof(clientId_), "%s-%06llx", name,
           static_cast<unsigned long long>(ESP.getEfuseMac() >> 24) & 0xFFFFFF);

  esp_mqtt_client_config_t config = {};
  config.broker.address.hostname = host;
  config.broker.address.port = port;
  config.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
  // An empty field means the broker allows anonymous clients.
  config.credentials.username = user[0] ? user : nullptr;
  config.credentials.client_id = clientId_;
  config.credentials.authentication.password = password[0] ? password : nullptr;
  config.session.keepalive = options.keepaliveS;
  config.network.reconnect_timeout_ms = static_cast<int>(options.retryIntervalMs);
  config.buffer.size = options.bufferSize;
  if (options.lastWillTopic) {
    config.session.last_will.topic = options.lastWillTopic;
    config.session.last_will.msg = options.lastWillPayload;
    config.session.last_will.qos = 1;
    config.session.last_will.retain = options.lastWillRetain;
  }

  client_ = esp_mqtt_client_init(&config);
  esp_mqtt_client_register_event(client_, MQTT_EVENT_ANY, onEvent, this);
  Serial.printf("MQTT: connecting to %s:%u as %s\n", host, port, clientId_);
  esp_mqtt_client_start(client_);
}

// Runs in the MQTT task, not the loop's. It only touches connected_ (atomic)
// and the inbox queue, both of which are safe to share.
void MqttConnection::onEvent(void* handlerArgs, esp_event_base_t, int32_t eventId, void* eventData) {
  auto* self = static_cast<MqttConnection*>(handlerArgs);
  auto* event = static_cast<esp_mqtt_event_handle_t>(eventData);

  switch (static_cast<esp_mqtt_event_id_t>(eventId)) {
    case MQTT_EVENT_CONNECTED:
      // Every connection starts a fresh session, so subscribe again. The
      // broker replies with each topic's retained message (its latest value).
      for (int i = 0; i < self->subscriptionCount_; i++) {
        esp_mqtt_client_subscribe_single(event->client, self->subscriptions_[i], 0);
      }
      self->connected_ = true;
      self->loginRefused_ = false;
      break;

    case MQTT_EVENT_DISCONNECTED:
      self->connected_ = false;
      break;

    case MQTT_EVENT_ERROR:
      // An unreachable broker just shows up as a disconnect (and a retry).
      // A refusal is worth spelling out: it's almost always the login.
      if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
        const int code = event->error_handle->connect_return_code;
        // 4 = bad username/password, 5 = not authorized. Both mean the login
        // is wrong, which the settings page can then say outright.
        self->loginRefused_ = code == 4 || code == 5;
        Serial.printf("MQTT: broker refused the connection (code %d; 4 or 5 means check "
                      "the MQTT login on the setup page)\n",
                      code);
      }
      break;

    case MQTT_EVENT_DATA: {
      // A message bigger than the buffer arrives in pieces. We don't expect
      // any, so rather than reassembling them, skip it and say so.
      if (event->data_len != event->total_data_len) {
        if (event->current_data_offset == 0) {
          Serial.printf("MQTT: skipped a %d-byte message (the buffer is %d)\n",
                        event->total_data_len, self->bufferSize_);
        }
        break;
      }

      // Copy it out: the client reuses its buffer as soon as we return.
      auto* block = static_cast<char*>(malloc(event->topic_len + 1 + event->data_len + 1));
      if (!block) break;
      InboxMessage message{block, block + event->topic_len + 1};
      memcpy(message.topic, event->topic, event->topic_len);
      message.topic[event->topic_len] = '\0';
      memcpy(message.payload, event->data, event->data_len);
      message.payload[event->data_len] = '\0';

      if (xQueueSend(self->inbox_, &message, 0) != pdTRUE) free(block);  // queue full: drop it
      break;
    }

    default:
      break;
  }
}

void MqttConnection::stop() {
  if (!client_) return;
  esp_mqtt_client_stop(client_);
  esp_mqtt_client_destroy(client_);
  client_ = nullptr;
  connected_ = false;
  loginRefused_ = false;
  reportedConnected_ = false;

  // Anything the old broker sent and we haven't read yet is no longer wanted.
  InboxMessage message;
  while (xQueueReceive(inbox_, &message, 0) == pdTRUE) free(message.topic);
  Serial.println("MQTT: stopped");
}

bool MqttConnection::tick(OnMessage onMessage, void* context) {
  if (!client_) return false;

  InboxMessage message;
  while (xQueueReceive(inbox_, &message, 0) == pdTRUE) {
    // A caller that hasn't subscribed to anything yet passes no handler; the
    // queue should be empty for it, but a message must still be freed rather
    // than called with nothing to call.
    if (onMessage) onMessage(context, message.topic, message.payload);
    free(message.topic);
  }

  const bool connected = connected_;
  if (connected == reportedConnected_) return false;
  reportedConnected_ = connected;
  Serial.println(connected ? "MQTT: connected" : "MQTT: disconnected, will retry");
  return true;
}

bool MqttConnection::publish(const char* topic, const char* payload, bool retain, int qos) {
  if (!connected_) return false;
  // store=true is what lets a QoS 0 message wait in the client's outbox for
  // its task to send, instead of being written here on the caller's task.
  return esp_mqtt_client_enqueue(client_, topic, payload, 0 /* length: strlen */, qos, retain,
                                 true /* store */) >= 0;
}
