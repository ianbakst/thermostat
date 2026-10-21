#pragma once
#include <WebServer.h>

#include "climate/heat_pump.h"
#include "net/mqtt_connection.h"
#include "net/page_login.h"
#include "net/wifi_connection.h"
#include "storage/settings_store.h"

// The controller's pages on the home network, at http://<name>.local/:
//
//   /          the controls (net/control_page.h), which work through the
//              control API (net/direct_api.h). Open to anyone on the network
//   /settings  what it's doing, and the same settings form as setup mode
//              (net/settings_form.h), so an installed unit can be checked on
//              and reconfigured without opening it up. Needs the settings
//              password (net/page_login.h)
//   /login     where that password is typed
//
// Firmware updates will go behind the password too.
//
// A unit set up before there was a password has none, and its settings stay
// shut until one is chosen in setup mode. Saved Wi-Fi and broker passwords
// never leave the controller, and anything that changes something is only
// accepted from its own pages (net/own_address.h), so another website can't
// have a browser post it.
class WebPages {
 public:
  // Starts serving, once the network is up. Only the first call does
  // anything, so it can sit next to the MQTT connection's begin() in
  // main.cpp. Everything passed in is read whenever a page is loaded, so it
  // has to outlive this.
  void begin(SettingsStore& store, const UnitSettings& settings, const HeatPumpLink& heatPump,
             const WifiConnection& wifi, const MqttConnection& mqtt, PageLogin& login);

  // Call from loop(). Serves whatever has been asked for since the last call.
  void tick();

 private:
  void handleControls();
  void handleSettings();
  void handleSave();
  void handleLoginPage();
  void handleLogin();
  void handleLogout();

  void showSettings(const UnitSettings& shown, const String& error);
  void showLogin(const String& message);
  void showNoPassword();
  void redirect(const char* path);
  bool fromOwnPage();
  String currentLogin();
  String addressNote();

  WebServer server_{80};
  SettingsStore* store_ = nullptr;
  const UnitSettings* settings_ = nullptr;
  const HeatPumpLink* heatPump_ = nullptr;
  const WifiConnection* wifi_ = nullptr;
  const MqttConnection* mqtt_ = nullptr;
  PageLogin* login_ = nullptr;
  bool started_ = false;
};
