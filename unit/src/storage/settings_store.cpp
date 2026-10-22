#include "storage/settings_store.h"

// NVS groups keys by namespace; keys are limited to 15 characters.
static const char* NAMESPACE = "minisplit";
static const char* KEY_SSID = "wifi_ssid";
static const char* KEY_WIFI_PASS = "wifi_pass";
static const char* KEY_NAME = "name";
static const char* KEY_MQTT_HOST = "mqtt_host";
static const char* KEY_MQTT_PORT = "mqtt_port";
static const char* KEY_MQTT_USER = "mqtt_user";
static const char* KEY_MQTT_PASS = "mqtt_pass";
static const char* KEY_FAHRENHEIT = "fahrenheit";
static const char* KEY_MIN_TEMP = "min_temp";
static const char* KEY_MAX_TEMP = "max_temp";
static const char* KEY_TEMP_STEP = "temp_step";
static const char* KEY_HA_UNIQUE_ID = "ha_uniq_id";
static const char* KEY_OTA_CHANNEL = "ota_channel";

void SettingsStore::begin() { prefs_.begin(NAMESPACE, false /* read-write */); }

// Leaves `out` alone (keeping its default) if the key was never saved.
static void loadString(Preferences& prefs, const char* key, char* out, size_t size) {
  if (prefs.isKey(key)) prefs.getString(key, out, size);
}

UnitSettings SettingsStore::load() {
  UnitSettings settings;
  loadString(prefs_, KEY_SSID, settings.wifiSsid, sizeof(settings.wifiSsid));
  loadString(prefs_, KEY_WIFI_PASS, settings.wifiPassword, sizeof(settings.wifiPassword));
  loadString(prefs_, KEY_NAME, settings.name, sizeof(settings.name));
  loadString(prefs_, KEY_MQTT_HOST, settings.mqttHost, sizeof(settings.mqttHost));
  loadString(prefs_, KEY_MQTT_USER, settings.mqttUser, sizeof(settings.mqttUser));
  loadString(prefs_, KEY_MQTT_PASS, settings.mqttPassword, sizeof(settings.mqttPassword));
  loadString(prefs_, KEY_HA_UNIQUE_ID, settings.haUniqueId, sizeof(settings.haUniqueId));
  settings.mqttPort = prefs_.getUShort(KEY_MQTT_PORT, settings.mqttPort);
  settings.useFahrenheit = prefs_.getBool(KEY_FAHRENHEIT, settings.useFahrenheit);
  settings.minTemp = prefs_.getFloat(KEY_MIN_TEMP, settings.minTemp);
  settings.maxTemp = prefs_.getFloat(KEY_MAX_TEMP, settings.maxTemp);
  settings.tempStep = prefs_.getFloat(KEY_TEMP_STEP, settings.tempStep);
  return settings;
}

void SettingsStore::save(const UnitSettings& settings) {
  prefs_.putString(KEY_SSID, settings.wifiSsid);
  prefs_.putString(KEY_WIFI_PASS, settings.wifiPassword);
  prefs_.putString(KEY_NAME, settings.name);
  prefs_.putString(KEY_MQTT_HOST, settings.mqttHost);
  prefs_.putUShort(KEY_MQTT_PORT, settings.mqttPort);
  prefs_.putString(KEY_MQTT_USER, settings.mqttUser);
  prefs_.putString(KEY_MQTT_PASS, settings.mqttPassword);
  prefs_.putString(KEY_HA_UNIQUE_ID, settings.haUniqueId);
  prefs_.putBool(KEY_FAHRENHEIT, settings.useFahrenheit);
  prefs_.putFloat(KEY_MIN_TEMP, settings.minTemp);
  prefs_.putFloat(KEY_MAX_TEMP, settings.maxTemp);
  prefs_.putFloat(KEY_TEMP_STEP, settings.tempStep);
}

OtaChannel SettingsStore::loadOtaChannel() {
  return prefs_.getUChar(KEY_OTA_CHANNEL, 0) == static_cast<uint8_t>(OtaChannel::Beta)
             ? OtaChannel::Beta
             : OtaChannel::Stable;
}

void SettingsStore::saveOtaChannel(OtaChannel channel) {
  prefs_.putUChar(KEY_OTA_CHANNEL, static_cast<uint8_t>(channel));
}
