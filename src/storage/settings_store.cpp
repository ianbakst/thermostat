#include "storage/settings_store.h"

// NVS groups keys by namespace; keys are limited to 15 characters.
static const char* NAMESPACE = "thermostat";
static const char* KEY_SSID = "wifi_ssid";
static const char* KEY_WIFI_PASS = "wifi_pass";
static const char* KEY_HOSTNAME = "hostname";
static const char* KEY_MQTT_HOST = "mqtt_host";
static const char* KEY_MQTT_PORT = "mqtt_port";
static const char* KEY_MQTT_USER = "mqtt_user";
static const char* KEY_MQTT_PASS = "mqtt_pass";
static const char* KEY_UNIT = "unit";
static const char* KEY_BRIGHTNESS = "brightness";

void SettingsStore::begin() { prefs_.begin(NAMESPACE, false /* read-write */); }

// Leaves `out` alone (keeping its default) if the key was never saved.
static void loadString(Preferences& prefs, const char* key, char* out, size_t size) {
  if (prefs.isKey(key)) prefs.getString(key, out, size);
}

DeviceSettings SettingsStore::load() {
  DeviceSettings settings;
  loadString(prefs_, KEY_SSID, settings.wifiSsid, sizeof(settings.wifiSsid));
  loadString(prefs_, KEY_WIFI_PASS, settings.wifiPassword, sizeof(settings.wifiPassword));
  loadString(prefs_, KEY_HOSTNAME, settings.hostname, sizeof(settings.hostname));
  loadString(prefs_, KEY_MQTT_HOST, settings.mqttHost, sizeof(settings.mqttHost));
  loadString(prefs_, KEY_MQTT_USER, settings.mqttUser, sizeof(settings.mqttUser));
  loadString(prefs_, KEY_MQTT_PASS, settings.mqttPassword, sizeof(settings.mqttPassword));
  loadString(prefs_, KEY_UNIT, settings.unitName, sizeof(settings.unitName));
  settings.mqttPort = prefs_.getUShort(KEY_MQTT_PORT, settings.mqttPort);
  settings.brightness = prefs_.getUChar(KEY_BRIGHTNESS, settings.brightness);
  return settings;
}

void SettingsStore::save(const DeviceSettings& settings) {
  prefs_.putString(KEY_SSID, settings.wifiSsid);
  prefs_.putString(KEY_WIFI_PASS, settings.wifiPassword);
  prefs_.putString(KEY_HOSTNAME, settings.hostname);
  prefs_.putString(KEY_MQTT_HOST, settings.mqttHost);
  prefs_.putUShort(KEY_MQTT_PORT, settings.mqttPort);
  prefs_.putString(KEY_MQTT_USER, settings.mqttUser);
  prefs_.putString(KEY_MQTT_PASS, settings.mqttPassword);
  prefs_.putString(KEY_UNIT, settings.unitName);
  prefs_.putUChar(KEY_BRIGHTNESS, settings.brightness);
}

void SettingsStore::saveUnitName(const char* name) { prefs_.putString(KEY_UNIT, name); }

void SettingsStore::saveBrightness(uint8_t brightness) {
  prefs_.putUChar(KEY_BRIGHTNESS, brightness);
}
