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

// The order here is the order options appear in the pickers, and the order the
// wire values are listed in mini_split.cpp — the two must match.
enum class HvacMode : uint8_t { Off, Heat, Cool, Auto, Dry, FanOnly };
enum class FanMode : uint8_t { Auto, Quiet, Low, Middle, Medium, High };

constexpr int HVAC_MODE_COUNT = 6;
constexpr int FAN_MODE_COUNT = 6;

// The louvres. Vertical is what Home Assistant shows as the climate entity's
// swing mode; horizontal isn't advertised to HA at all, so this panel is the
// only convenient way to set it.
//
// Vane 1 is the flattest position and 5 points furthest down. Wide vane runs
// left to right, and Spread ("<>") splits the airflow both ways.
enum class VaneMode : uint8_t { Auto, Pos1, Pos2, Pos3, Pos4, Pos5, Swing };
enum class WideVaneMode : uint8_t { FarLeft, Left, Center, Right, FarRight, Spread, Swing };

constexpr int VANE_MODE_COUNT = 7;
constexpr int WIDE_VANE_MODE_COUNT = 7;

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

// Which options a unit actually has, one bit per enum value, from the lists in
// its Home Assistant discovery config. Not every unit has every mode: this
// firmware only advertises "heat" on units that can heat.
inline bool isSupported(uint8_t mask, int index) { return (mask >> index) & 1u; }

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
  VaneMode vane = VaneMode::Auto;
  WideVaneMode wideVane = WideVaneMode::Center;
  HvacAction action = HvacAction::Idle;  // reported every 30s

  // Everything is on offer until the unit's config says otherwise. The
  // horizontal louvre has no advertised list, so it has no mask.
  uint8_t modeMask = 0xFF;
  uint8_t fanMask = 0xFF;
  uint8_t vaneMask = 0xFF;
};

const char* modeName(HvacMode mode);
const char* fanModeName(FanMode fan);
const char* vaneName(VaneMode vane);
const char* wideVaneName(WideVaneMode wideVane);

// Whether the setpoint means anything in this mode (not in Off, Dry or Fan).
bool usesTargetTemp(HvacMode mode);
