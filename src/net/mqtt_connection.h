#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <mqtt_client.h>

#include <atomic>

// A connection to the MQTT broker, using the login in include/secrets.h.
//
// This uses ESP-IDF's MQTT client, which is built into the Arduino core. It
// runs in its own FreeRTOS task: connecting, reconnecting after drops and
// sending all happen there, so a slow or unreachable broker never stalls the
// UI. Received messages are queued and handed over in tick(), on the loop's
// task, so the code that uses them doesn't have to deal with threads.
class MqttConnection {
 public:
  using OnMessage = void (*)(void* context, const char* topic, const char* payload);

  // Adds a topic to subscribe to each time we connect. Call before begin().
  // `topic` must stay valid (e.g. a string literal).
  void subscribe(const char* topic);

  // Starts connecting (needs Wi-Fi). The broker and login are the saved
  // settings, which live for as long as the program does; `name` (the panel's
  // name) starts the client id the broker logs us under. Only the first call
  // does anything; after that the client reconnects by itself every
  // MQTT_RETRY_INTERVAL_MS.
  void begin(const char* host, uint16_t port, const char* user, const char* password,
             const char* name);

  // Call from loop(). Passes each message received since the last call to
  // onMessage (the payload is null-terminated). Returns true when we've just
  // connected or disconnected.
  bool tick(OnMessage onMessage, void* context);

  bool connected() const { return connected_; }

  // True when the broker answered "not authorized": the login is wrong, and
  // retrying won't help until it's changed on the setup page.
  bool loginRefused() const { return loginRefused_; }

  // Sends a (non-retained) message. It's queued for the MQTT task to send, so
  // this never blocks. Returns false if we're not connected.
  bool publish(const char* topic, const char* payload);

  // Disconnects and forgets the broker, so begin() can run again with a
  // different one. Setup mode uses this to try a login the user just typed.
  // Subscriptions are kept: they're re-sent on the next connection.
  void stop();

 private:
  static void onEvent(void* handlerArgs, esp_event_base_t base, int32_t eventId, void* eventData);

  static constexpr int MAX_SUBSCRIPTIONS = 8;
  const char* subscriptions_[MAX_SUBSCRIPTIONS] = {};
  int subscriptionCount_ = 0;

  esp_mqtt_client_handle_t client_ = nullptr;
  QueueHandle_t inbox_ = nullptr;       // messages from the MQTT task
  std::atomic<bool> connected_{false};     // set by the MQTT task
  std::atomic<bool> loginRefused_{false};  // set by the MQTT task
  bool reportedConnected_ = false;      // what tick() last reported
  char clientId_[32] = "";
};
