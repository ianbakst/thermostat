#pragma once
#include <lvgl.h>

#include <cstdint>

#include "climate/climate.h"
#include "config.h"
#include "net/ota_status.h"
#include "net/wifi_status.h"
#include "ui/airflow_screen.h"
#include "ui/list_screen.h"
#include "ui/picker.h"
#include "ui/settings_screen.h"
#include "ui/setup_screen.h"

// The thermostat screen (portrait, 172x320):
//
//   [⚙]    72°F    📶   settings, current temperature, Wi-Fi signal
//
//         ▲
//        70°F           target temperature; arrows (+/-) step it
//         ▼
//
//   [mode] [air] [fan]  icons show the current setting; tap to pick
//
// The background color shows the current mode and, for Heat, Cool and Dry,
// whether the unit is running: orange while heating, dark red when idle;
// blue while cooling, deep blue when idle; teal while drying, dark teal when
// idle. Black for Off. Until the mini-split is online, everything is faded
// and the controls are locked.
//
// The UI never changes anything itself. A tap calls one of the Callbacks with
// what the user asked for; main.cpp acts on it and calls show() with the
// result. The settings page leads to the other screens: setup (a QR code for
// the panel's own Wi-Fi network) and the list of mini-splits to pick from.
class Ui {
 public:
  // One callback per control. The climate ones are separate because the
  // mini-split takes each as a separate command (resending all of them would
  // make it beep for each).
  struct Callbacks {
    void (*setTargetTemp)(float temp);
    void (*setMode)(HvacMode mode);
    void (*setFanMode)(FanMode fan);
    void (*setVane)(VaneMode vane);              // vertical louvre, from the airflow page
    void (*setWideVane)(WideVaneMode wideVane);  // horizontal louvre
    void (*startWifiSetup)();        // "Wi-Fi setup" on the settings page
    void (*startBrokerSetup)();      // "MQTT setup": the same wizard from step 2
    void (*cancelSetup)();           // the X on the setup screen
    void (*openUnitList)();          // "Change" under Mini-split: main calls showUnits()
    void (*setUnit)(const char* name);
    void (*checkForUpdate)();        // "Check for updates" under Firmware
    void (*installUpdate)();         // the same button, once there's one to install
    void (*openChannelList)();       // "Channel": main calls showChannels()
    void (*setOtaChannel)(OtaChannel channel);
    void (*setBrightness)(uint8_t value, bool save);  // the brightness slider
  };

  // Requires Display::begin() to have run.
  void begin(const Callbacks& callbacks);

  void show(const ClimateState& state);

  // Shown as the top-right Wi-Fi icon and on the settings page.
  void showWifiStatus(const WifiStatus& status);

  // Shown on the settings page.
  void showMqttStatus(const char* broker, bool connected, bool loginRefused);
  void showBrightness(uint8_t brightness);
  void showOta(const OtaStatus& status, OtaChannel channel);

  // Setup mode: the QR code and what it says in words (see SetupScreen).
  void showSetup(const char* qrData, const char* line1, const char* line2, const char* hint,
                 bool cancellable);
  void showSetupProgress(const char* text);

  // The mini-splits to pick from (from MiniSplit).
  void showUnits(const char* const* names, int count, const char* selected);

  // The two update channels, as a list like the one above.
  void showChannels(OtaChannel current);

  // Covers everything while the panel restarts, so a black screen doesn't
  // look like a crash.
  void showRestarting();

 private:
  void onStep(int direction);
  void openSettings();
  void openModePicker();
  void openFanPicker();
  void openAirflow();
  void openVaneList();
  void openWideVaneList();

  Callbacks callbacks_{};
  ClimateState shown_;  // what's on screen; taps are computed from this
  uint8_t brightness_ = DEFAULT_BRIGHTNESS;  // for the settings page's slider
  Picker picker_;
  SettingsScreen settingsScreen_;
  SetupScreen setupScreen_;
  AirflowScreen airflowScreen_;
  ListScreen listScreen_;

  // Filled in when a picker or list opens, to turn the row the user tapped
  // back into the value it stands for. Only the options the unit actually has
  // are listed, so a row's position isn't its enum value.
  HvacMode pickerModes_[HVAC_MODE_COUNT]{};
  FanMode pickerFans_[FAN_MODE_COUNT]{};
  VaneMode vaneOptions_[VANE_MODE_COUNT]{};
  const char* vaneLabels_[VANE_MODE_COUNT]{};
  const char* wideVaneLabels_[WIDE_VANE_MODE_COUNT]{};
  const char* const* unitNames_ = nullptr;

  lv_obj_t* screen_ = nullptr;
  lv_obj_t* currentLabel_ = nullptr;
  lv_obj_t* wifiIcon_ = nullptr;
  lv_obj_t* targetLabel_ = nullptr;
  lv_obj_t* upButton_ = nullptr;
  lv_obj_t* downButton_ = nullptr;
  lv_obj_t* modeButton_ = nullptr;
  lv_obj_t* modeIcon_ = nullptr;
  lv_obj_t* fanButton_ = nullptr;
  lv_obj_t* fanIcon_ = nullptr;
  lv_obj_t* airflowButton_ = nullptr;
};
