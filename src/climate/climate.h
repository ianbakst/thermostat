#pragma once
//
// The state of one mini-split, as the UI sees it.
//
// Modelled on Home Assistant's climate entity so that step 4 (MQTT) maps
// straight onto it: HA's hvac modes are off / heat / cool / heat_cool (or
// auto) / dry / fan_only, and fan modes are free-form strings like auto /
// low / medium / high.
//
// Temperatures are always stored in °C. The display unit (°F or °C) only
// affects how they're shown and how far one +/- tap moves the setpoint.
//

#include <cmath>
#include <cstdint>

enum class HvacMode : uint8_t { Off, Heat, Cool, Auto, Dry, FanOnly };
enum class FanSpeed : uint8_t { Auto, Low, Medium, High };

struct ClimateState {
  float currentTempC = NAN;  // room temperature reported by the mini-split
  float targetTempC = NAN;   // setpoint
  HvacMode mode = HvacMode::Off;
  FanSpeed fan = FanSpeed::Auto;
};

const char* modeName(HvacMode mode);
const char* fanSpeedName(FanSpeed fan);

// For tap-to-cycle buttons: Off -> Heat -> Cool -> ... -> Off.
HvacMode nextMode(HvacMode mode);
FanSpeed nextFanSpeed(FanSpeed fan);

// Whether the setpoint means anything in this mode (not in Off or Fan).
bool usesTargetTemp(HvacMode mode);
