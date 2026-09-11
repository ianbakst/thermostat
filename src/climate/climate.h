#pragma once
//
// The state of one mini-split, as the UI sees it.
//
// Modelled on Home Assistant's climate entity so that step 4 (MQTT) maps
// straight onto it: HA's hvac modes are off / heat / cool / heat_cool (or
// auto) / dry / fan_only, and fan modes are whatever strings the mini-split
// integration reports (here: low / medium / high / auto / off / ambient).
//
// Temperatures are always stored in °C. The display unit (°F or °C) only
// affects how they're shown and how far one +/- tap moves the setpoint.
//

#include <cmath>
#include <cstdint>

// The order here is the order options appear in the pickers.
enum class HvacMode : uint8_t { Off, Heat, Cool, Auto, Dry, FanOnly };
enum class FanMode : uint8_t { Low, Medium, High, Auto, Off, Ambient };

constexpr int HVAC_MODE_COUNT = 6;
constexpr int FAN_MODE_COUNT = 6;

struct ClimateState {
  float currentTempC = NAN;  // room temperature reported by the mini-split
  float targetTempC = NAN;   // setpoint
  HvacMode mode = HvacMode::Off;
  FanMode fan = FanMode::Auto;
};

const char* modeName(HvacMode mode);
const char* fanModeName(FanMode fan);

// Whether the setpoint means anything in this mode (not in Off or Fan).
bool usesTargetTemp(HvacMode mode);
