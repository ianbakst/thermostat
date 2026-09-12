#pragma once
#include <cmath>

#include "climate/climate.h"
#include "config.h"
#include "net/mqtt_connection.h"

// One setting the panel can change (setpoint, mode or fan). A change shows
// on screen immediately. Until the unit reports that value back, reports
// still carrying the old one are ignored, so the screen doesn't jump back
// while the unit catches up. After PENDING_HOLD_MS the latest report wins
// either way (e.g. if the unit refused the change).
template <typename T>
struct PendingValue {
  bool pending = false;
  unsigned long sinceMs = 0;
  bool hasReport = false;
  T reported{};  // latest value from the unit

  static bool same(T a, T b) { return a == b; }

  // Sets `shown` to `value`. Returns true if that changed it.
  static bool update(T& shown, T value) {
    if (same(shown, value)) return false;
    shown = value;
    return true;
  }

  // The user asked for `value`.
  void request(T& shown, T value, unsigned long nowMs) {
    shown = value;
    pending = true;
    sinceMs = nowMs;
  }

  // The unit reported `value`. Returns true if `shown` changed.
  bool report(T& shown, T value) {
    reported = value;
    hasReport = true;
    if (pending) {
      if (!same(shown, value)) return false;  // still the old value
      pending = false;
    }
    return update(shown, value);
  }

  // Stops waiting after PENDING_HOLD_MS. Returns true if `shown` changed.
  bool expire(T& shown, unsigned long nowMs) {
    if (!pending || nowMs - sinceMs < PENDING_HOLD_MS) return false;
    pending = false;
    return hasReport && update(shown, reported);
  }
};

// Temperatures are compared loosely (float rounding), and unknown equals unknown.
template <>
inline bool PendingValue<float>::same(float a, float b) {
  return std::fabs(a - b) < 0.01f || (std::isnan(a) && std::isnan(b));
}

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

  PendingValue<float> target_;
  PendingValue<HvacMode> mode_;
  PendingValue<FanMode> fan_;
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

  // Units seen on the discovery topic. unitNames_ points into units_ so the
  // list can be handed to the UI as it is.
  char units_[MINI_SPLIT_MAX_UNITS][33] = {};
  const char* unitNames_[MINI_SPLIT_MAX_UNITS] = {};
  int unitCount_ = 0;
};
