#pragma once
#include <lvgl.h>

#include "climate/climate.h"
#include "climate/temperature.h"

// The thermostat screen (portrait, 172x320):
//
//   name
//   current temperature (large)
//   [-]   target   [+]
//   [ Mode: Heat ]      tap to cycle
//   [ Fan: Auto  ]      tap to cycle
//
// The UI never changes the climate state itself. A tap calls one of the
// Callbacks with what the user asked for; main.cpp decides what to do and
// calls show() with the result. For now main.cpp just applies the change
// straight away. In step 4 it will send it to Home Assistant, and show()
// will run when the mini-split reports its new state.
class Ui {
 public:
  // One callback per control, because Home Assistant takes each as a separate
  // command. (Resending all of them would make the mini-split beep for each.)
  struct Callbacks {
    void (*setTargetTemp)(float celsius);
    void (*setMode)(HvacMode mode);
    void (*setFanSpeed)(FanSpeed fan);
  };

  // Requires Display::begin() to have run.
  void begin(const char* name, TempUnit unit, const Callbacks& callbacks);

  void show(const ClimateState& state);

 private:
  void onStep(int direction);
  void onModeTap();
  void onFanTap();

  TempUnit unit_ = TempUnit::Fahrenheit;
  Callbacks callbacks_{};
  ClimateState shown_;  // what's on screen; taps are computed from this

  lv_obj_t* currentLabel_ = nullptr;
  lv_obj_t* targetLabel_ = nullptr;
  lv_obj_t* minusButton_ = nullptr;
  lv_obj_t* plusButton_ = nullptr;
  lv_obj_t* modeButton_ = nullptr;
  lv_obj_t* modeLabel_ = nullptr;
  lv_obj_t* fanLabel_ = nullptr;
};
