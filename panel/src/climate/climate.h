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
#include "protocol/mini_split_protocol.h"

// The modes, fan speeds and louvre positions (HvacMode, FanMode, VaneMode,
// WideVaneMode, HvacAction and their counts) are the protocol's vocabulary,
// so they live in the shared header above, next to the strings they travel
// as. The order of each enum is the order its options appear in the pickers.

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
