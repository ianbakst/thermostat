#pragma once
#include <lvgl.h>

#include <cstdint>

#include "climate/climate.h"
#include "net/wifi_status.h"
#include "ui/overlay_screen.h"

// The settings page (portrait, 172x320). The title bar stays put and
// everything below it scrolls, because there's more here than fits:
//
//   [X] Settings            X closes the page
//   - - - - - - - - - - -   scrolls below here
//   Screen Brightness
//   [==========|-------]    backlight, live as you drag
//
//   Wi-Fi
//   MyNetwork
//   192.168.1.40
//   Signal: -55 dBm (strong)
//   [ Wi-Fi setup ]         the panel's own setup network, from step 1
//
//   MQTT
//   homeassistant.local:1883
//   Connected
//   [ MQTT setup ]          broker settings only; needs Wi-Fi, so it's
//                           greyed out without it
//   Mini-split
//   OfficeHVAC
//   Online
//   [ Change ]              pick from the units Home Assistant advertises
//
// The page is built when opened and deleted when closed. While it's closed it
// just remembers the latest status to show next time. Like the rest of the UI
// it does nothing itself: the buttons and the slider call back to the owner.
class SettingsScreen : public OverlayScreen {
 public:
  struct Callbacks {
    void (*startWifiSetup)(void* context);
    void (*startBrokerSetup)(void* context);
    void (*openUnitList)(void* context);
    // While dragging, `save` is false: the brightness follows the slider but
    // isn't written to flash until the finger lifts.
    void (*setBrightness)(void* context, uint8_t value, bool save);
  };

  void open(const Callbacks& callbacks, void* context, uint8_t brightness);

  void showWifiStatus(const WifiStatus& status);
  void showMqtt(const char* broker, bool connected, bool loginRefused);
  void showClimate(ClimateLink link, const char* unitName);

 private:
  // The show* methods above store what they're given; these draw the stored
  // values. Keeping the two apart means opening the page never has to pass a
  // member back into its own setter (which would copy a buffer onto itself).
  void renderWifi();
  void renderMqtt();
  void renderClimate();
  void close();

  lv_obj_t* wifiLabel_ = nullptr;
  lv_obj_t* mqttLabel_ = nullptr;
  lv_obj_t* mqttButton_ = nullptr;
  lv_obj_t* climateLabel_ = nullptr;

  WifiStatus wifiStatus_;
  char broker_[80] = "";
  bool mqttConnected_ = false;
  bool mqttLoginRefused_ = false;
  ClimateLink climateLink_ = ClimateLink::NoUnit;
  char unitName_[33] = "";

  Callbacks callbacks_{};
  void* context_ = nullptr;
};
