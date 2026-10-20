#pragma once
#include <WebServer.h>

#include "climate/heat_pump.h"
#include "net/mqtt_connection.h"
#include "net/wifi_connection.h"
#include "storage/settings_store.h"

// The controller's pages on the home network, at http://<name>.local/:
//
//   /          the controls (net/control_page.h), which work through the
//              control API (net/direct_api.h)
//   /settings  what it's doing, and the same settings form as setup mode
//              (net/settings_form.h), so an installed unit can be checked on
//              and reconfigured without opening it up
//
// Firmware updates will go here too.
//
// There's no login yet: anyone on the home network can use the controls and
// change the settings, as with the firmware these units ran before. Saved
// passwords never leave the controller, and a save is only accepted from its
// own page (net/own_address.h), so another website can't have a browser post
// one. A login has to come before this page can install firmware.
class WebPages {
 public:
  // Starts serving, once the network is up. Only the first call does
  // anything, so it can sit next to the MQTT connection's begin() in
  // main.cpp. Everything passed in is read whenever a page is loaded, so it
  // has to outlive this.
  void begin(SettingsStore& store, const UnitSettings& settings, const HeatPumpLink& heatPump,
             const WifiConnection& wifi, const MqttConnection& mqtt);

  // Call from loop(). Serves whatever has been asked for since the last call.
  void tick();

 private:
  void handleControls();
  void handleSettings();
  void handleSave();
  String addressNote();

  WebServer server_{80};
  SettingsStore* store_ = nullptr;
  const UnitSettings* settings_ = nullptr;
  const HeatPumpLink* heatPump_ = nullptr;
  const WifiConnection* wifi_ = nullptr;
  const MqttConnection* mqtt_ = nullptr;
  bool started_ = false;
};
