#pragma once
//
// The state of one mini-split, as the UI sees it.
//
// Modelled on Home Assistant's climate entity: HA's hvac modes are off /
// heat / cool / heat_cool / dry / fan_only, and these units' fan modes are
// auto / diffuse (quiet) / low / middle / medium / high.
//
// Temperatures are in the mini-split's own unit (°F or °C, as set on the
// unit), exactly as it reports them. The panel never converts them.
//

#include <cmath>
#include <cstdint>

#include "climate/temperature.h"

// The order here is the order options appear in the pickers.
enum class HvacMode : uint8_t { Off, Heat, Cool, Auto, Dry, FanOnly };
enum class FanMode : uint8_t { Auto, Quiet, Low, Middle, Medium, High };

constexpr int HVAC_MODE_COUNT = 6;
constexpr int FAN_MODE_COUNT = 6;

// What the unit is doing right now, which can differ from its mode: in Heat
// it's Heating while the compressor runs and Idle once the room is warm
// enough. The same values as Home Assistant's hvac_action.
enum class HvacAction : uint8_t { Off, Idle, Heating, Cooling, Drying, Fan };
constexpr int HVAC_ACTION_COUNT = 6;

// How far along we are in hearing from the mini-split.
enum class ClimateLink : uint8_t {
  NoUnit,      // no mini-split picked yet (Settings -> Mini-split)
  Connecting,  // not connected to the MQTT broker (or no Wi-Fi yet)
  Refused,     // the broker rejected the login: it needs fixing in setup
  Waiting,     // connected; waiting for the unit's details
  Offline,     // the unit itself isn't connected (it reports "offline")
  Online,      // up to date; the controls work
};

struct ClimateState {
  ClimateLink link = ClimateLink::NoUnit;
  const char* unitName = "";  // the unit this panel controls, e.g. "OfficeHVAC"

  TempUnit unit = TempUnit::Fahrenheit;  // as set on the unit
  float currentTemp = NAN;  // room temperature
  float targetTemp = NAN;   // setpoint
  float minTemp = NAN;      // setpoint limits and +/- step, from the unit
  float maxTemp = NAN;
  float tempStep = 1.0f;

  HvacMode mode = HvacMode::Off;
  FanMode fan = FanMode::Auto;
  HvacAction action = HvacAction::Idle;  // reported every 30s
};

const char* modeName(HvacMode mode);
const char* fanModeName(FanMode fan);

// Whether the setpoint means anything in this mode (not in Off, Dry or Fan).
bool usesTargetTemp(HvacMode mode);
