#pragma once
#include <Preferences.h>

#include <cstdint>
#include <cstdio>

#include "config.h"

// Everything the panel is told about its installation. All of it is entered
// on the setup page (see src/net/provisioning.h), except the mini-split,
// which is picked on the panel once Home Assistant's units are known. None of
// it is compiled into the firmware, so the same build runs on any panel.
struct DeviceSettings {
  // The two text defaults are copied in rather than being initializers: the
  // config values are typed constants now, and only a string literal can
  // initialize a char array.
  DeviceSettings() {
    snprintf(hostname, sizeof(hostname), "%s", DEFAULT_HOSTNAME);
    snprintf(mqttHost, sizeof(mqttHost), "%s", DEFAULT_MQTT_HOST);
  }

  char wifiSsid[33] = "";
  char wifiPassword[64] = "";
  char hostname[33] = "";
  char mqttHost[64] = "";
  uint16_t mqttPort = DEFAULT_MQTT_PORT;
  char mqttUser[33] = "";
  char mqttPassword[64] = "";
  char unitName[33] = "";  // e.g. "OfficeHVAC"; empty until one is picked
  uint8_t brightness = DEFAULT_BRIGHTNESS;  // backlight while awake, from the settings page

  // Whether there's a network to try. Without one, the panel starts in setup
  // mode.
  bool hasNetwork() const { return wifiSsid[0] != '\0'; }
};

// The settings as kept in the ESP32's flash (NVS), so they survive reboots,
// power cuts and reflashing the firmware.
class SettingsStore {
 public:
  void begin();

  // Anything never saved keeps the default above.
  DeviceSettings load();

  void save(const DeviceSettings& settings);
  void saveUnitName(const char* name);
  void saveBrightness(uint8_t brightness);

 private:
  Preferences prefs_;
};
