#pragma once
#include <lvgl.h>

#include "climate/climate.h"
#include "climate/temperature.h"
#include "ui/picker.h"
#include "ui/settings_screen.h"

// The thermostat screen (portrait, 172x320):
//
//   [⚙]       72°F      settings, current temperature
//
//         ▲
//        70°F           target temperature; arrows (+/-) step it
//         ▼
//
//   [mode]    [fan]     icons show the current setting; tap to pick
//
// The UI never changes the climate state itself. A tap calls one of the
// Callbacks with what the user asked for; main.cpp decides what to do and
// calls show() with the result. For now main.cpp just applies the change
// straight away. In step 4 it will send it to Home Assistant, and show()
// will run when the mini-split reports its new state.
class Ui {
 public:
  // One callback per control. The climate ones are separate because Home
  // Assistant takes each as a separate command (resending all of them would
  // make the mini-split beep for each).
  struct Callbacks {
    void (*setTargetTemp)(float celsius);
    void (*setMode)(HvacMode mode);
    void (*setFanMode)(FanMode fan);
    void (*setTempUnit)(TempUnit unit);  // from the settings page
  };

  // Requires Display::begin() to have run.
  void begin(TempUnit unit, const Callbacks& callbacks);

  void show(const ClimateState& state);

  // Redraws every temperature in the new unit (and the settings page's
  // selection, if it's open).
  void showTempUnit(TempUnit unit);

 private:
  void onStep(int direction);
  void openSettings();
  void openModePicker();
  void openFanPicker();

  TempUnit unit_ = TempUnit::Fahrenheit;
  Callbacks callbacks_{};
  ClimateState shown_;  // what's on screen; taps are computed from this
  Picker picker_;
  SettingsScreen settingsScreen_;

  lv_obj_t* currentLabel_ = nullptr;
  lv_obj_t* targetLabel_ = nullptr;
  lv_obj_t* upButton_ = nullptr;
  lv_obj_t* downButton_ = nullptr;
  lv_obj_t* modeButton_ = nullptr;
  lv_obj_t* modeIcon_ = nullptr;
  lv_obj_t* fanIcon_ = nullptr;
};
