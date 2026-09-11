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
    case FanMode::Low:     return "Low";
    case FanMode::Medium:  return "Medium";
    case FanMode::High:    return "High";
    case FanMode::Auto:    return "Auto";
    case FanMode::Off:     return "Off";
    case FanMode::Ambient: return "Ambient";
  }
  return "?";
}

bool usesTargetTemp(HvacMode mode) {
  return mode != HvacMode::Off && mode != HvacMode::FanOnly;
}
