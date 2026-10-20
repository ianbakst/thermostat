#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <mqtt_client.h>

#include <atomic>

// A connection to the MQTT broker, shared by both firmwares.
//
// This uses ESP-IDF's MQTT client, which is built into the Arduino core. It
// runs in its own FreeRTOS task: connecting, reconnecting after drops and
// sending all happen there, so a slow or unreachable broker never stalls the
// caller — the panel's UI stays live, and the mini-split keeps talking to its
// heat pump. Received messages are queued and handed over in tick(), on the
// loop's task, so the code that uses them doesn't have to deal with threads.
class MqttConnection {
 public:
  using OnMessage = void (*)(void* context, const char* topic, const char* payload);

  // Timings and buffer sizes, which each firmware sets from its own config.h,
  // plus the optional last will: the message the broker publishes on our
  // behalf if we disappear without saying goodbye. The mini-split's
  // controller uses it to mark itself offline, so a panel finds out even when
  // the controller has lost power. The strings must stay valid for as long as
  // the connection does.
  struct Options {
    int keepaliveS = 30;                   // the broker drops us after 1.5x this in silence
    unsigned long retryIntervalMs = 5'000;  // wait between connection attempts
    int bufferSize = 4096;                 // the largest message we can send or receive
    const char* lastWillTopic = nullptr;   // nullptr for no last will
    const char* lastWillPayload = "";
    bool lastWillRetain = true;
  };

  // Adds a topic to subscribe to each time we connect. Call before begin().
  // `topic` must stay valid (e.g. a string literal).
  void subscribe(const char* topic);

  // Starts connecting (needs Wi-Fi). The broker and login are the saved
  // settings, which live for as long as the program does; `name` (the
  // device's name) starts the client id the broker logs us under. Only the
  // first call does anything; after that the client reconnects by itself
  // every `options.retryIntervalMs`.
  void begin(const char* host, uint16_t port, const char* user, const char* password,
             const char* name, const Options& options);

  // Call from loop(). Passes each message received since the last call to
  // onMessage (the payload is null-terminated). Returns true when we've just
  // connected or disconnected.
  //
  // onMessage may be null, for a device that has subscribed to nothing and
  // only wants to know whether the broker is there.
  bool tick(OnMessage onMessage, void* context);

  bool connected() const { return connected_; }

  // True when the broker answered "not authorized": the login is wrong, and
  // retrying won't help until it's changed on the setup page.
  bool loginRefused() const { return loginRefused_; }

  // Sends a message. It's queued for the MQTT task to send, so this never
  // blocks. Returns false if we're not connected.
  //
  // `retain` asks the broker to keep it as the topic's latest value and hand
  // it to whoever subscribes next. That's what lets a panel show real state
  // the moment it connects instead of waiting for the next update, so the
  // mini-split's controller retains its state and availability. A command
  // ("set the mode to heat") must NOT be retained, or it would be replayed at
  // every future subscriber.
  bool publish(const char* topic, const char* payload, bool retain = false, int qos = 0);

  // Disconnects and forgets the broker, so begin() can run again with a
  // different one. Setup mode uses this to try a login the user just typed.
  // Subscriptions are kept: they're re-sent on the next connection.
  void stop();

 private:
  static void onEvent(void* handlerArgs, esp_event_base_t base, int32_t eventId, void* eventData);

  static constexpr int MAX_SUBSCRIPTIONS = 8;
  const char* subscriptions_[MAX_SUBSCRIPTIONS] = {};
  int subscriptionCount_ = 0;
  int bufferSize_ = 0;  // kept only to name the limit in the oversize-message log

  esp_mqtt_client_handle_t client_ = nullptr;
  QueueHandle_t inbox_ = nullptr;       // messages from the MQTT task
  std::atomic<bool> connected_{false};     // set by the MQTT task
  std::atomic<bool> loginRefused_{false};  // set by the MQTT task
  bool reportedConnected_ = false;      // what tick() last reported
  char clientId_[32] = "";
};
