#pragma once
#include <cmath>

#include "climate/climate.h"
#include "climate/tracked.h"
#include "config.h"
#include "net/mqtt_connection.h"

// The mini-split this panel controls, through the MQTT topics its
// mitsubishi2mqtt firmware uses. These are the same topics and values Home
// Assistant uses, so the panel and HA always agree.
//
// Which unit that is comes from the saved settings, so the topics are built at
// startup. For a unit named <name>:
//
//   homeassistant/climate/+/config         every unit's HA discovery config.
//                                         The wildcard is how the panel lists
//                                         the units to pick from; the chosen
//                                         one's gives its temperature unit,
//                                         setpoint limits and step
//   mitsubishi2mqtt/<name>/availability    "online" / "offline" (retained)
//   mitsubishi2mqtt/<name>/settings        {"temperature":77,"fan":"auto","mode":"off",...}
//                                          (retained; sent after every change)
//   mitsubishi2mqtt/<name>/state           the same plus "roomTemperature" and
//                                          "action" (heating, idle, ...) (every 30s)
//
//   mitsubishi2mqtt/<name>/mode/set        we send off, heat, cool, heat_cool, dry, fan_only
//   mitsubishi2mqtt/<name>/temp/set        we send the setpoint, e.g. 72
//   mitsubishi2mqtt/<name>/fan/set         we send auto, diffuse, low, middle, medium, high
//   mitsubishi2mqtt/<name>/vane/set        we send AUTO, 1-5, SWING (the vertical louvre,
//                                          which HA shows as the swing mode)
//   mitsubishi2mqtt/<name>/wideVane/set    we send <<, <, |, >, >>, <>, SWING (the horizontal
//                                          louvre, which HA doesn't expose at all)
class MiniSplit {
 public:
  explicit MiniSplit(MqttConnection& mqtt) : mqtt_(mqtt) {}

  // Registers the topics above for `unitName` (which may be empty: the panel
  // then only listens for the units to pick from). Call before
  // MqttConnection::begin().
  void begin(const char* unitName);

  // Call from loop(). Returns true when state() has changed.
  bool tick(unsigned long nowMs);

  const ClimateState& state() const { return state_; }

  // The units Home Assistant has advertised since startup, for picking one.
  int unitCount() const { return unitCount_; }
  const char* const* unitNames() const { return unitNames_; }

  // What the user asked for. Ignored unless the unit is online.
  void setTargetTemp(float temp, unsigned long nowMs);
  void setMode(HvacMode mode, unsigned long nowMs);
  void setFanMode(FanMode fan, unsigned long nowMs);
  void setVane(VaneMode vane, unsigned long nowMs);
  void setWideVane(WideVaneMode wideVane, unsigned long nowMs);

 private:
  void onMessage(const char* topic, const char* payload);
  void addUnit(const char* name);
  void readConfig(const char* payload);
  void readSettings(const char* payload);
  void sendTargetTemp(unsigned long nowMs);
  void publish(const char* topic, const char* payload);
  ClimateLink currentLink() const;

  MqttConnection& mqtt_;
  ClimateState state_;

  // Bound to the state_ fields above, which they keep up to date.
  Tracked<float> target_{state_.targetTemp};
  Tracked<HvacMode> mode_{state_.mode};
  Tracked<FanMode> fan_{state_.fan};
  Tracked<VaneMode> vane_{state_.vane};
  Tracked<WideVaneMode> wideVane_{state_.wideVane};
  bool targetSendDue_ = false;  // a setpoint change is waiting for the taps to pause

  bool hasConfig_ = false;
  bool availabilityKnown_ = false;
  bool unitOnline_ = false;
  bool changed_ = false;  // during tick(): whether state_ has changed

  // The chosen unit and its topics, built in begin().
  char unitName_[33] = "";
  char availabilityTopic_[96] = "";
  char settingsTopic_[96] = "";
  char stateTopic_[96] = "";
  char modeSetTopic_[96] = "";
  char tempSetTopic_[96] = "";
  char fanSetTopic_[96] = "";
  char vaneSetTopic_[96] = "";
  char wideVaneSetTopic_[96] = "";

  // Units seen on the discovery topic. unitNames_ points into units_ so the
  // list can be handed to the UI as it is.
  char units_[MINI_SPLIT_MAX_UNITS][33] = {};
  const char* unitNames_[MINI_SPLIT_MAX_UNITS] = {};
  int unitCount_ = 0;
};
