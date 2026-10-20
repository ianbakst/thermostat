#pragma once
#include <Arduino.h>

#include "climate/heat_pump.h"
#include "net/mqtt_connection.h"
#include "storage/settings_store.h"

// This unit on the MQTT broker: what Home Assistant and the panels see of it,
// and the changes they send. The topics and payloads are mitsubishi2mqtt's,
// written down in mini_split_protocol.h, so neither can tell the firmware
// changed. For a unit named <name>:
//
//   homeassistant/climate/<name>/config   HA's discovery config, so the unit
//                                         turns up in HA by itself. Retained,
//                                         sent on every connection
//   mitsubishi2mqtt/<name>/availability   "online" while the heat pump
//                                         answers, "offline" while it doesn't,
//                                         and the last will. Retained
//   mitsubishi2mqtt/<name>/settings       retained, whenever a setting changes
//   mitsubishi2mqtt/<name>/state          the settings plus the room and what
//                                         the unit is doing, on every change
//                                         and every MQTT_STATE_INTERVAL_MS
//   mitsubishi2mqtt/<name>/+/set          changes, which get the same checks
//                                         and the same HeatPumpLink::apply()
//                                         as the control page's
//
// Like the control API (net/direct_api.h), it runs from loop() and only ever
// reports what the unit reports: a change shows up once the unit has
// confirmed it, a few seconds later.
class MqttApi {
 public:
  // Subscribes to the /set topics, sets the last will, and starts the
  // connection with the saved broker and login. Call once the network is up;
  // only the first call does anything. All three are used for as long as the
  // program runs.
  void begin(MqttConnection& mqtt, HeatPumpLink& heatPump, const UnitSettings& settings,
             MqttConnection::Options options);

  // Call from loop(). Acts on what the broker has sent, announces the unit
  // each time the broker connects, and sends the periodic state.
  void tick(unsigned long nowMs);

  // Sends what's changed. Call whenever the heat pump's state does.
  void publish(unsigned long nowMs);

 private:
  void onMessage(const char* topic, const char* payload);
  void clearRenamedTopics();
  void publishConfig();

  MqttConnection* mqtt_ = nullptr;
  HeatPumpLink* heatPump_ = nullptr;
  const UnitSettings* settings_ = nullptr;
  bool started_ = false;

  // The topics, built in begin(). A name is at most 32 characters.
  char base_[64] = "";          // mitsubishi2mqtt/<name>
  char commandTopic_[72] = "";  // <base>/+/set
  char availabilityTopic_[80] = "";
  char settingsTopic_[80] = "";
  char stateTopic_[80] = "";
  char configTopic_[80] = "";
  char uniqueId_[33] = "";

  // What this connection has been told, so only changes are sent. Cleared
  // when the broker connects: it may have restarted and lost what it held.
  bool availabilitySent_ = false;
  bool online_ = false;
  String lastSettings_;
  unsigned long lastStateMs_ = 0;
};
