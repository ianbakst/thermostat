#pragma once
#include <Preferences.h>

#include <cstdint>
#include <cstdio>

#include "config.h"
#include "net/ota_status.h"

// Everything this controller is told about its installation, all of it
// entered on the setup page (see src/net/provisioning.h). None of it is
// compiled into the firmware, so the same build runs on every unit.
//
// The panel has a store of its own with the same shape. They aren't shared:
// the two devices hold different things, and what they'd have in common is
// the handful of lines below that call Preferences.
struct UnitSettings {
  UnitSettings() {
    snprintf(name, sizeof(name), "%s", DEFAULT_UNIT_NAME);
    snprintf(mqttHost, sizeof(mqttHost), "%s", DEFAULT_MQTT_HOST);
  }

  char wifiSsid[33] = "";
  char wifiPassword[64] = "";

  // What this mini-split is called: its hostname, its <name>.local address,
  // and the middle of every topic it uses — "mitsubishi2mqtt/<name>/state".
  // Home Assistant and the panel know the unit by this, so replacing the
  // board inside a unit means reusing its name.
  char name[33] = "";

  char mqttHost[64] = "";
  uint16_t mqttPort = DEFAULT_MQTT_PORT;
  char mqttUser[33] = "";
  char mqttPassword[64] = "";

  // How this unit reports temperatures, and the setpoint range it advertises.
  // The heat pump itself always works in °C; this is the unit everything
  // above sees, and what a panel reflects rather than converting.
  bool useFahrenheit = DEFAULT_USE_FAHRENHEIT;
  float minTemp = DEFAULT_MIN_TEMP;  // in the unit above
  float maxTemp = DEFAULT_MAX_TEMP;
  float tempStep = DEFAULT_TEMP_STEP;

  // What Home Assistant uses to recognise this unit's entity. Empty means
  // "derive one from this board's chip id", which is what the ESP8266
  // firmware did — so a replacement board would arrive as a *new* entity and
  // leave the old dashboards pointing at nothing. Putting the old board's id
  // here instead makes the swap invisible to HA. See README.md.
  char haUniqueId[33] = "";

  // Whether there's a network to try. Without one, the controller starts its
  // own and serves the setup page.
  bool hasNetwork() const { return wifiSsid[0] != '\0'; }
};

// The settings as kept in the ESP32's flash (NVS), so they survive reboots,
// power cuts and reflashing the firmware.
class SettingsStore {
 public:
  void begin();

  // Anything never saved keeps the default above.
  UnitSettings load();

  void save(const UnitSettings& settings);

  // Which releases it's offered, saved on its own: it's changed from the
  // settings page without a restart, so it isn't in UnitSettings, where
  // saving the form would put back whatever it was at boot.
  OtaChannel loadOtaChannel();
  void saveOtaChannel(OtaChannel channel);

 private:
  Preferences prefs_;
};
