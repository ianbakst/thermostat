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

const char* fanSpeedName(FanSpeed fan) {
  switch (fan) {
    case FanSpeed::Auto:   return "Auto";
    case FanSpeed::Low:    return "Low";
    case FanSpeed::Medium: return "Medium";
    case FanSpeed::High:   return "High";
  }
  return "?";
}

HvacMode nextMode(HvacMode mode) {
  const int count = static_cast<int>(HvacMode::FanOnly) + 1;
  return static_cast<HvacMode>((static_cast<int>(mode) + 1) % count);
}

FanSpeed nextFanSpeed(FanSpeed fan) {
  const int count = static_cast<int>(FanSpeed::High) + 1;
  return static_cast<FanSpeed>((static_cast<int>(fan) + 1) % count);
}

bool usesTargetTemp(HvacMode mode) {
  return mode != HvacMode::Off && mode != HvacMode::FanOnly;
}
