#include "climate/climate.h"

const char* modeName(HvacMode mode) {
  switch (mode) {
    case HvacMode::Off:     return "Off";
    case HvacMode::Heat:    return "Heat";
    case HvacMode::Cool:    return "Cool";
    case HvacMode::Auto:    return "Auto";
    case HvacMode::Dry:     return "Dry";
    case HvacMode::FanOnly: return "Fan";
  }
  return "?";
}

const char* fanModeName(FanMode fan) {
  switch (fan) {
    case FanMode::Auto:    return "Auto";
    case FanMode::Quiet:   return "Quiet";
    case FanMode::Low:     return "Low";
    case FanMode::Middle:  return "Middle";
    case FanMode::Medium:  return "Medium";
    case FanMode::High:    return "High";
  }
  return "?";
}

bool usesTargetTemp(HvacMode mode) {
  // Dry has no setpoint of its own: the unit decides when to dehumidify
  // ("Temperature cannot be set during DRY mode" in Mitsubishi's manual).
  // The unit still reports a temperature — the setpoint Heat/Cool will use.
  return mode != HvacMode::Off && mode != HvacMode::FanOnly && mode != HvacMode::Dry;
}
