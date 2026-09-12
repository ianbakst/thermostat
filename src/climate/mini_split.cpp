#include "climate/mini_split.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Every mini-split's discovery config. Subscribing with a wildcard is how the
// panel finds out which units exist, so one can be picked on screen.
static const char* const CONFIG_PREFIX = HA_DISCOVERY_PREFIX "/climate/";
static const char* const CONFIG_SUFFIX = "/config";
static const char* const CONFIG_WILDCARD = HA_DISCOVERY_PREFIX "/climate/+/config";

// The values used on the wire, in the same order as the enums.
static const char* const MODE_VALUES[HVAC_MODE_COUNT] = {"off", "heat", "cool", "heat_cool", "dry", "fan_only"};
static const char* const FAN_VALUES[FAN_MODE_COUNT] = {"auto", "diffuse", "low", "middle", "medium", "high"};
static const char* const ACTION_VALUES[HVAC_ACTION_COUNT] = {"off", "idle", "heating", "cooling", "drying", "fan"};
static const char* const VANE_VALUES[VANE_MODE_COUNT] = {"AUTO", "1", "2", "3", "4", "5", "SWING"};
static const char* const WIDE_VANE_VALUES[WIDE_VANE_MODE_COUNT] = {"<<", "<", "|", ">", ">>", "<>", "SWING"};

// Position of `value` in one of the tables above, or -1.
static int indexOf(const char* const* values, int count, const char* value) {
  for (int i = 0; i < count; i++) {
    if (strcmp(values[i], value) == 0) return i;
  }
  return -1;
}

// Which entries of a table a unit advertises, one bit each. An absent or
// unrecognised list means "assume it has everything", which is how the panel
// behaved before it read these at all.
static uint8_t maskFromArray(JsonArrayConst array, const char* const* values, int count) {
  if (array.isNull()) return 0xFF;
  uint8_t mask = 0;
  for (JsonVariantConst item : array) {
    const char* value = item.as<const char*>();
    if (!value) continue;
    const int index = indexOf(values, count, value);
    if (index >= 0) mask |= static_cast<uint8_t>(1u << index);
  }
  return mask ? mask : 0xFF;
}

static int countBits(uint8_t mask) {
  int count = 0;
  for (int i = 0; i < 8; i++) count += (mask >> i) & 1u;
  return count;
}

// Pulls <name> out of "homeassistant/climate/<name>/config".
static bool discoveryUnitName(const char* topic, char* out, size_t size) {
  const size_t prefixLength = strlen(CONFIG_PREFIX);
  if (strncmp(topic, CONFIG_PREFIX, prefixLength) != 0) return false;
  const char* suffix = strstr(topic + prefixLength, CONFIG_SUFFIX);
  if (!suffix || suffix[strlen(CONFIG_SUFFIX)] != '\0') return false;

  const size_t length = suffix - (topic + prefixLength);
  if (length == 0 || length >= size) return false;
  memcpy(out, topic + prefixLength, length);
  out[length] = '\0';
  return true;
}

static const char* linkName(ClimateLink link) {
  switch (link) {
    case ClimateLink::NoUnit:     return "not chosen yet";
    case ClimateLink::Connecting: return "connecting";
    case ClimateLink::Refused:    return "MQTT login refused";
    case ClimateLink::Waiting:    return "waiting for the unit";
    case ClimateLink::Offline:    return "offline";
    case ClimateLink::Online:     return "online";
  }
  return "?";
}

void MiniSplit::begin(const char* unitName) {
  snprintf(unitName_, sizeof(unitName_), "%s", unitName);
  state_.unitName = unitName_;

  mqtt_.subscribe(CONFIG_WILDCARD);
  if (unitName_[0] == '\0') return;  // nothing to follow until one is picked

  snprintf(availabilityTopic_, sizeof(availabilityTopic_), "%s/%s/availability",
           MINI_SPLIT_TOPIC_PREFIX, unitName_);
  snprintf(settingsTopic_, sizeof(settingsTopic_), "%s/%s/settings", MINI_SPLIT_TOPIC_PREFIX, unitName_);
  snprintf(stateTopic_, sizeof(stateTopic_), "%s/%s/state", MINI_SPLIT_TOPIC_PREFIX, unitName_);
  snprintf(modeSetTopic_, sizeof(modeSetTopic_), "%s/%s/mode/set", MINI_SPLIT_TOPIC_PREFIX, unitName_);
  snprintf(tempSetTopic_, sizeof(tempSetTopic_), "%s/%s/temp/set", MINI_SPLIT_TOPIC_PREFIX, unitName_);
  snprintf(fanSetTopic_, sizeof(fanSetTopic_), "%s/%s/fan/set", MINI_SPLIT_TOPIC_PREFIX, unitName_);
  snprintf(vaneSetTopic_, sizeof(vaneSetTopic_), "%s/%s/vane/set", MINI_SPLIT_TOPIC_PREFIX, unitName_);
  snprintf(wideVaneSetTopic_, sizeof(wideVaneSetTopic_), "%s/%s/wideVane/set", MINI_SPLIT_TOPIC_PREFIX,
           unitName_);

  mqtt_.subscribe(availabilityTopic_);
  mqtt_.subscribe(settingsTopic_);
  mqtt_.subscribe(stateTopic_);
}

bool MiniSplit::tick(unsigned long nowMs) {
  changed_ = false;

  const bool connectionChanged = mqtt_.tick(
      [](void* context, const char* topic, const char* payload) {
        static_cast<MiniSplit*>(context)->onMessage(topic, payload);
      },
      this);
  // Availability is retained, so the broker resends it when we reconnect.
  // Until then we can't know it.
  if (connectionChanged && !mqtt_.connected()) availabilityKnown_ = false;

  if (targetSendDue_ && nowMs - target_.heldSinceMs() >= SETPOINT_SEND_DELAY_MS) {
    sendTargetTemp(nowMs);
  }

  changed_ |= target_.expire(nowMs);
  changed_ |= mode_.expire(nowMs);
  changed_ |= fan_.expire(nowMs);
  changed_ |= vane_.expire(nowMs);
  changed_ |= wideVane_.expire(nowMs);

  const ClimateLink link = currentLink();
  if (link != state_.link) {
    state_.link = link;
    Serial.printf("Mini-split: %s %s\n", unitName_[0] ? unitName_ : "(no unit)", linkName(link));
    changed_ = true;
  }

  return changed_;
}

ClimateLink MiniSplit::currentLink() const {
  if (unitName_[0] == '\0') return ClimateLink::NoUnit;
  if (!mqtt_.connected()) {
    return mqtt_.loginRefused() ? ClimateLink::Refused : ClimateLink::Connecting;
  }
  if (availabilityKnown_ && !unitOnline_) return ClimateLink::Offline;
  if (!availabilityKnown_ || !hasConfig_ || !target_.hasReport()) return ClimateLink::Waiting;
  return ClimateLink::Online;
}

// ---------------------------------------------------------------------------
// From the unit
// ---------------------------------------------------------------------------

void MiniSplit::onMessage(const char* topic, const char* payload) {
  char unit[sizeof(unitName_)];
  if (discoveryUnitName(topic, unit, sizeof(unit))) {
    addUnit(unit);
    if (strcmp(unit, unitName_) == 0) readConfig(payload);
    return;
  }

  if (strcmp(topic, availabilityTopic_) == 0) {
    availabilityKnown_ = true;
    unitOnline_ = strcmp(payload, "online") == 0;
  } else if (strcmp(topic, settingsTopic_) == 0 || strcmp(topic, stateTopic_) == 0) {
    readSettings(payload);
  }
}

void MiniSplit::addUnit(const char* name) {
  for (int i = 0; i < unitCount_; i++) {
    if (strcmp(units_[i], name) == 0) return;
  }
  if (unitCount_ >= MINI_SPLIT_MAX_UNITS) return;  // MINI_SPLIT_MAX_UNITS in config.h

  snprintf(units_[unitCount_], sizeof(units_[0]), "%s", name);
  unitNames_[unitCount_] = units_[unitCount_];
  unitCount_++;
  changed_ = true;
  Serial.printf("Mini-split: Home Assistant has %s\n", name);
}

void MiniSplit::readConfig(const char* payload) {
  // The config is ~2.4KB; only keep the fields we use.
  JsonDocument filter;
  filter["temperature_unit"] = true;
  filter["min_temp"] = true;
  filter["max_temp"] = true;
  filter["temp_step"] = true;
  // Which modes, fan speeds and vane positions this unit actually has. Not
  // every unit has every mode: mitsubishi2mqtt only advertises "heat" on the
  // ones that can heat.
  filter["modes"][0] = true;
  filter["fan_modes"][0] = true;
  filter["swing_modes"][0] = true;

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
  if (error) {
    Serial.printf("Mini-split: unreadable config (%s)\n", error.c_str());
    return;
  }

  state_.unit = doc["temperature_unit"] == "C" ? TempUnit::Celsius : TempUnit::Fahrenheit;
  state_.minTemp = doc["min_temp"] | NAN;
  state_.maxTemp = doc["max_temp"] | NAN;
  // mitsubishi2mqtt sends the step as a string ("1"), though HA also accepts
  // a number. HA's default is 1.
  const JsonVariantConst step = doc["temp_step"];
  const float stepValue = step.is<const char*>() ? strtof(step.as<const char*>(), nullptr) : step.as<float>();
  state_.tempStep = stepValue > 0 ? stepValue : 1.0f;

  state_.modeMask = maskFromArray(doc["modes"], MODE_VALUES, HVAC_MODE_COUNT);
  state_.fanMask = maskFromArray(doc["fan_modes"], FAN_VALUES, FAN_MODE_COUNT);
  state_.vaneMask = maskFromArray(doc["swing_modes"], VANE_VALUES, VANE_MODE_COUNT);

  hasConfig_ = !std::isnan(state_.minTemp) && !std::isnan(state_.maxTemp);
  changed_ = true;

  char min[16], max[16];
  formatTemp(min, sizeof(min), state_.minTemp, state_.unit, state_.tempStep);
  formatTemp(max, sizeof(max), state_.maxTemp, state_.unit, state_.tempStep);
  Serial.printf("Mini-split: setpoint range %s to %s, step %g; offers %d modes, %d fan speeds, "
                "%d vane positions\n",
                min, max, state_.tempStep, countBits(state_.modeMask), countBits(state_.fanMask),
                countBits(state_.vaneMask));
}

void MiniSplit::readSettings(const char* payload) {
  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.printf("Mini-split: unreadable message (%s)\n", error.c_str());
    return;
  }

  bool changed = false;

  // Only in "state" messages. mitsubishi2mqtt uses 0 for "not read yet".
  const float room = doc["roomTemperature"] | NAN;
  if (room > 0) changed |= assignIfChanged(state_.currentTemp, room);

  if (doc["temperature"].is<float>()) {
    changed |= target_.report(doc["temperature"].as<float>());
  }
  const int mode = indexOf(MODE_VALUES, HVAC_MODE_COUNT, doc["mode"] | "");
  if (mode >= 0) changed |= mode_.report(static_cast<HvacMode>(mode));
  const int fan = indexOf(FAN_VALUES, FAN_MODE_COUNT, doc["fan"] | "");
  if (fan >= 0) changed |= fan_.report(static_cast<FanMode>(fan));
  const int vane = indexOf(VANE_VALUES, VANE_MODE_COUNT, doc["vane"] | "");
  if (vane >= 0) changed |= vane_.report(static_cast<VaneMode>(vane));
  const int wideVane = indexOf(WIDE_VANE_VALUES, WIDE_VANE_MODE_COUNT, doc["wideVane"] | "");
  if (wideVane >= 0) changed |= wideVane_.report(static_cast<WideVaneMode>(wideVane));

  // Only in "state" messages. Right after a mode change mitsubishi2mqtt
  // guesses (e.g. "cooling" for Cool); its next 30s report has the real one.
  const int action = indexOf(ACTION_VALUES, HVAC_ACTION_COUNT, doc["action"] | "");
  if (action >= 0) changed |= assignIfChanged(state_.action, static_cast<HvacAction>(action));

  if (!changed) return;
  changed_ = true;

  char roomText[16], targetText[16];
  formatTemp(roomText, sizeof(roomText), state_.currentTemp, state_.unit, state_.tempStep);
  formatTemp(targetText, sizeof(targetText), state_.targetTemp, state_.unit, state_.tempStep);
  Serial.printf("Mini-split: room %s, target %s, mode %s (%s), fan %s, vane %s, wide %s\n",
                roomText, targetText, modeName(state_.mode),
                ACTION_VALUES[static_cast<int>(state_.action)], fanModeName(state_.fan),
                VANE_VALUES[static_cast<int>(state_.vane)],
                WIDE_VANE_VALUES[static_cast<int>(state_.wideVane)]);
}

// ---------------------------------------------------------------------------
// To the unit
// ---------------------------------------------------------------------------

void MiniSplit::setTargetTemp(float temp, unsigned long nowMs) {
  if (state_.link != ClimateLink::Online) return;

  // mitsubishi2mqtt replaces an out-of-range setpoint with 23°C, so never
  // send one.
  temp = std::clamp(temp, state_.minTemp, state_.maxTemp);
  target_.request(temp, nowMs);
  targetSendDue_ = true;  // tick() sends it once the taps pause
}

void MiniSplit::sendTargetTemp(unsigned long nowMs) {
  targetSendDue_ = false;
  char payload[16];
  snprintf(payload, sizeof(payload), "%g", state_.targetTemp);
  publish(tempSetTopic_, payload);
  target_.holdFrom(nowMs);  // hold from when it was sent, not the last tap
}

void MiniSplit::setMode(HvacMode mode, unsigned long nowMs) {
  if (state_.link != ClimateLink::Online || mode == state_.mode) return;
  mode_.request(mode, nowMs);
  publish(modeSetTopic_, MODE_VALUES[static_cast<int>(mode)]);
}

void MiniSplit::setFanMode(FanMode fan, unsigned long nowMs) {
  if (state_.link != ClimateLink::Online || fan == state_.fan) return;
  fan_.request(fan, nowMs);
  publish(fanSetTopic_, FAN_VALUES[static_cast<int>(fan)]);
}

void MiniSplit::setVane(VaneMode vane, unsigned long nowMs) {
  if (state_.link != ClimateLink::Online || vane == state_.vane) return;
  vane_.request(vane, nowMs);
  publish(vaneSetTopic_, VANE_VALUES[static_cast<int>(vane)]);
}

void MiniSplit::setWideVane(WideVaneMode wideVane, unsigned long nowMs) {
  if (state_.link != ClimateLink::Online || wideVane == state_.wideVane) return;
  wideVane_.request(wideVane, nowMs);
  publish(wideVaneSetTopic_, WIDE_VANE_VALUES[static_cast<int>(wideVane)]);
}

void MiniSplit::publish(const char* topic, const char* payload) {
  const bool queued = mqtt_.publish(topic, payload);
  Serial.printf("Mini-split: %s %s%s\n", topic, payload, queued ? "" : " (NOT sent: MQTT disconnected)");
}
