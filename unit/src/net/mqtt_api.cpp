#include "net/mqtt_api.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>

#include <cctype>
#include <cstdlib>
#include <cstring>

#include "config.h"
#include "net/protocol_json.h"

// The name this unit last announced itself under, so a rename can clear what
// the old one left on the broker. Its own NVS namespace, since it's not
// something anyone enters.
static const char* PREFS_NAMESPACE = "mqtt_api";
static const char* PREFS_ANNOUNCED = "announced";

// Which /set topic changes which setting, by the setting's key in the
// protocol's JSON. Turning a message into that JSON means it's checked by
// protocol_json::readCommand(), the same as one from the control page.
struct Setter {
  const char* topic;
  const char* key;
};
static const Setter SETTERS[] = {
    {TOPIC_MODE_SET, KEY_MODE}, {TOPIC_TEMP_SET, KEY_TEMPERATURE},    {TOPIC_FAN_SET, KEY_FAN},
    {TOPIC_VANE_SET, KEY_VANE}, {TOPIC_WIDE_VANE_SET, KEY_WIDE_VANE},
};

// "mitsubishi2mqtt/<name>/<suffix>", for the suffixes the protocol names.
static void topicFor(char* out, size_t size, const char* name, const char* suffix) {
  snprintf(out, size, "%s/%s/%s", MINI_SPLIT_TOPIC_PREFIX, name, suffix);
}

static void configTopicFor(char* out, size_t size, const char* name) {
  snprintf(out, size, "%s%s%s", DISCOVERY_PREFIX, name, DISCOVERY_SUFFIX);
}

void MqttApi::begin(MqttConnection& mqtt, HeatPumpLink& heatPump, const UnitSettings& settings,
                    MqttConnection::Options options) {
  if (started_) return;
  started_ = true;
  mqtt_ = &mqtt;
  heatPump_ = &heatPump;
  settings_ = &settings;

  snprintf(base_, sizeof(base_), "%s/%s", MINI_SPLIT_TOPIC_PREFIX, settings.name);
  snprintf(commandTopic_, sizeof(commandTopic_), "%s/+/set", base_);
  topicFor(availabilityTopic_, sizeof(availabilityTopic_), settings.name, TOPIC_AVAILABILITY);
  topicFor(settingsTopic_, sizeof(settingsTopic_), settings.name, TOPIC_SETTINGS);
  topicFor(stateTopic_, sizeof(stateTopic_), settings.name, TOPIC_STATE);
  configTopicFor(configTopic_, sizeof(configTopic_), settings.name);

  // What HA knows the entity by: the old board's, if it was entered during
  // setup, or else this board's chip id, worked out the way mitsubishi2mqtt
  // did on an ESP32.
  if (settings.haUniqueId[0]) {
    snprintf(uniqueId_, sizeof(uniqueId_), "%s", settings.haUniqueId);
  } else {
    snprintf(uniqueId_, sizeof(uniqueId_), "%lx",
             static_cast<unsigned long>(static_cast<uint32_t>(ESP.getEfuseMac() >> 24)));
  }

  // If the controller drops off without a word — power cut, crash, Wi-Fi
  // gone — the broker says "offline" for it.
  options.lastWillTopic = availabilityTopic_;
  options.lastWillPayload = PAYLOAD_OFFLINE;
  options.lastWillRetain = true;

  // One wildcard rather than a subscription per setting: onMessage() tells
  // them apart.
  mqtt.subscribe(commandTopic_);
  mqtt.begin(settings.mqttHost, settings.mqttPort, settings.mqttUser, settings.mqttPassword,
             settings.name, options);
}

void MqttApi::tick(unsigned long nowMs) {
  if (!started_) return;

  const bool connectionChanged = mqtt_->tick(
      [](void* context, const char* topic, const char* payload) {
        static_cast<MqttApi*>(context)->onMessage(topic, payload);
      },
      this);

  if (connectionChanged) {
    availabilitySent_ = false;
    lastSettings_ = "";
    if (mqtt_->connected()) {
      clearRenamedTopics();
      publishConfig();
      publish(nowMs);
    }
  } else if (nowMs - lastStateMs_ >= MQTT_STATE_INTERVAL_MS) {
    publish(nowMs);
  }
}

void MqttApi::publish(unsigned long nowMs) {
  lastStateMs_ = nowMs;
  if (!started_ || !mqtt_->connected()) return;

  const HeatPumpState& state = heatPump_->state();

  if (state.connected) {
    JsonDocument doc;
    protocol_json::writeSettings(doc.to<JsonObject>(), state, *settings_);
    String settings;
    serializeJson(doc, settings);
    if (settings != lastSettings_) {
      mqtt_->publish(settingsTopic_, settings.c_str(), true /* retain */);
      lastSettings_ = settings;
    }

    protocol_json::writeStatus(doc.as<JsonObject>(), state, *settings_);
    String status;
    serializeJson(doc, status);
    mqtt_->publish(stateTopic_, status.c_str());
  }

  // Online only while there's a heat pump to report on and to take changes.
  // Without one, HA shows the unit as unavailable and a panel says it's
  // offline, instead of both showing the last settings as if they were live.
  if (!availabilitySent_ || state.connected != online_) {
    availabilitySent_ = true;
    online_ = state.connected;
    mqtt_->publish(availabilityTopic_, online_ ? PAYLOAD_ONLINE : PAYLOAD_OFFLINE, true);
    Serial.printf("MQTT: %s is %s\n", settings_->name,
                  online_ ? "online" : "offline until the heat pump answers");
  }
}

// ---------------------------------------------------------------------------
// From the broker
// ---------------------------------------------------------------------------

void MqttApi::onMessage(const char* topic, const char* payload) {
  // "<base>/<setting>/set": what follows the base says which setting.
  const size_t baseLength = strlen(base_);
  if (strncmp(topic, base_, baseLength) != 0 || topic[baseLength] != '/') return;
  const char* suffix = topic + baseLength + 1;

  const Setter* setter = nullptr;
  for (const Setter& candidate : SETTERS) {
    if (strcmp(suffix, candidate.topic) == 0) setter = &candidate;
  }
  if (!setter) {
    // remote_temp/set, and mitsubishi2mqtt's debugging and reboot topics.
    Serial.printf("MQTT: ignored %s, which this controller doesn't take\n", suffix);
    return;
  }

  JsonDocument doc;
  if (strcmp(setter->key, KEY_TEMPERATURE) == 0) {
    // The setpoint comes as text: "72", or "72.0" from HA. Anything that isn't
    // entirely a number stays text, which readCommand() turns down.
    char* end = nullptr;
    const float value = strtof(payload, &end);
    while (isspace(static_cast<unsigned char>(*end))) end++;
    if (end != payload && *end == '\0') {
      doc[setter->key] = value;
    } else {
      doc[setter->key] = payload;
    }
  } else {
    doc[setter->key] = payload;
  }

  HeatPumpCommand command;
  const char* error = protocol_json::readCommand(doc.as<JsonObjectConst>(), *settings_, command);
  if (!error && !heatPump_->state().connected) error = "The heat pump isn't connected.";
  if (error) {
    Serial.printf("MQTT: ignored %s \"%s\": %s\n", suffix, payload, error);
    return;
  }

  Serial.printf("MQTT: change from the broker: %s %s\n", suffix, payload);
  heatPump_->apply(command);  // which logs whether the unit took it
}

// ---------------------------------------------------------------------------
// What the unit says about itself
// ---------------------------------------------------------------------------

// A rename leaves the old name's retained messages on the broker. HA would
// keep the old entity and turn the new one away for having the same
// unique_id, and panels would go on listing the old name. So the name last
// announced is kept, and if it isn't this one, its retained messages are
// cleared first: an empty retained message deletes one, and an empty
// discovery config deletes the entity.
void MqttApi::clearRenamedTopics() {
  Preferences prefs;
  prefs.begin(PREFS_NAMESPACE, false /* read-write */);

  char previous[sizeof(UnitSettings::name)] = "";
  if (prefs.isKey(PREFS_ANNOUNCED)) prefs.getString(PREFS_ANNOUNCED, previous, sizeof(previous));

  if (strcmp(previous, settings_->name) != 0) {
    if (previous[0]) {
      char topic[sizeof(configTopic_)];
      configTopicFor(topic, sizeof(topic), previous);
      mqtt_->publish(topic, "", true);
      topicFor(topic, sizeof(topic), previous, TOPIC_SETTINGS);
      mqtt_->publish(topic, "", true);
      topicFor(topic, sizeof(topic), previous, TOPIC_AVAILABILITY);
      mqtt_->publish(topic, "", true);
      Serial.printf("MQTT: renamed from %s, so cleared what that name left on the broker\n",
                    previous);
    }
    prefs.putString(PREFS_ANNOUNCED, settings_->name);
  }
  prefs.end();
}

// The discovery config: everything HA needs to make a climate entity of this
// unit. It follows mitsubishi2mqtt's — the same keys, in HA's abbreviations,
// and the same device — so the entity HA already has carries on. The
// differences are noted where they are.
void MqttApi::publishConfig() {
  const UnitSettings& settings = *settings_;

  // HA expands a topic starting with "~" to this, which keeps the config
  // well inside the MQTT buffer. mitsubishi2mqtt spelled every topic out.
  const auto own = [](const char* suffix) { return String("~/") + suffix; };
  // mitsubishi2mqtt's templates fell back to defaults when a key was
  // missing. This controller always sends every key, and null for an
  // unknown room temperature, which HA shows as unknown.
  const auto field = [](const char* key) { return String("{{ value_json.") + key + " }}"; };
  const auto addAll = [](JsonDocument& doc, const char* key, const char* const* values, int count) {
    JsonArray list = doc[key].to<JsonArray>();
    for (int i = 0; i < count; i++) list.add(values[i]);
  };

  JsonDocument doc;
  doc["~"] = base_;
  doc["name"] = nullptr;  // the entity takes the device's name
  doc["unique_id"] = uniqueId_;

  doc["avty_t"] = own(TOPIC_AVAILABILITY);
  doc["pl_avail"] = PAYLOAD_ONLINE;
  doc["pl_not_avail"] = PAYLOAD_OFFLINE;

  doc["temperature_unit"] = settings.useFahrenheit ? "F" : "C";
  doc["min_temp"] = settings.minTemp;
  doc["max_temp"] = settings.maxTemp;
  doc["temp_step"] = settings.tempStep;

  // The settings come from the retained `settings` topic, where
  // mitsubishi2mqtt used `state`: HA shows them the moment it starts, rather
  // than after the next periodic state up to 30 seconds later.
  // mitsubishi2mqtt only listed "heat" for units set up as able to heat;
  // every mode is listed here.
  addAll(doc, "modes", MODE_VALUES, HVAC_MODE_COUNT);
  doc["mode_cmd_t"] = own(TOPIC_MODE_SET);
  doc["mode_stat_t"] = own(TOPIC_SETTINGS);
  doc["mode_stat_tpl"] = field(KEY_MODE);

  doc["temp_cmd_t"] = own(TOPIC_TEMP_SET);
  doc["temp_stat_t"] = own(TOPIC_SETTINGS);
  doc["temp_stat_tpl"] = field(KEY_TEMPERATURE);

  addAll(doc, "fan_modes", FAN_VALUES, FAN_MODE_COUNT);
  doc["fan_mode_cmd_t"] = own(TOPIC_FAN_SET);
  doc["fan_mode_stat_t"] = own(TOPIC_SETTINGS);
  doc["fan_mode_stat_tpl"] = field(KEY_FAN);

  addAll(doc, "swing_modes", VANE_VALUES, VANE_MODE_COUNT);
  doc["swing_mode_cmd_t"] = own(TOPIC_VANE_SET);
  doc["swing_mode_stat_t"] = own(TOPIC_SETTINGS);
  doc["swing_mode_stat_tpl"] = field(KEY_VANE);

  // Not in mitsubishi2mqtt's: the wide vane, as HA's horizontal swing mode.
  // HA versions from before that feature drop these keys and carry on.
  addAll(doc, "swing_horizontal_modes", WIDE_VANE_VALUES, WIDE_VANE_MODE_COUNT);
  doc["swing_horizontal_mode_command_topic"] = own(TOPIC_WIDE_VANE_SET);
  doc["swing_horizontal_mode_state_topic"] = own(TOPIC_SETTINGS);
  doc["swing_horizontal_mode_state_template"] = field(KEY_WIDE_VANE);

  doc["curr_temp_t"] = own(TOPIC_STATE);
  doc["curr_temp_tpl"] = field(KEY_ROOM_TEMPERATURE);
  doc["action_topic"] = own(TOPIC_STATE);
  doc["action_template"] = field(KEY_ACTION);

  // The compressor's speed, as an attribute of the entity.
  doc["json_attr_t"] = own(TOPIC_STATE);
  doc["json_attr_tpl"] = String("{{ {'") + KEY_COMPRESSOR_FREQUENCY + "': value_json." +
                         KEY_COMPRESSOR_FREQUENCY + "} | tojson }}";

  // mitsubishi2mqtt's device, so HA's device page reads the same. Its id is
  // the unit's name, which is what keeps the device when the board changes.
  JsonObject device = doc["device"].to<JsonObject>();
  device["ids"] = settings.name;
  device["name"] = settings.name;
  device["sw"] = "Mini-split controller " FIRMWARE_VERSION;
  device["mdl"] = "HVAC MITSUBISHI";
  device["mf"] = "MITSUBISHI ELECTRIC";
  device["configuration_url"] = String("http://") + WiFi.localIP().toString() + "/";

  String json;
  serializeJson(doc, json);
  const bool sent = mqtt_->publish(configTopic_, json.c_str(), true);
  Serial.printf("MQTT: %s Home Assistant's discovery config (%u bytes)\n",
                sent ? "sent" : "couldn't send", json.length());
}
