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

// Words rather than the unit's own "1".."5": on a wall panel "3 of 5" says
// more than a bare digit, and the ends are worth naming.
const char* vaneName(VaneMode vane) {
  switch (vane) {
    case VaneMode::Auto:  return "Auto";
    case VaneMode::Pos1:  return "1 - flattest";
    case VaneMode::Pos2:  return "2";
    case VaneMode::Pos3:  return "3";
    case VaneMode::Pos4:  return "4";
    case VaneMode::Pos5:  return "5 - straight down";
    case VaneMode::Swing: return "Swing";
  }
  return "?";
}

// The unit calls these "<<", "<", "|", ">", ">>", "<>" and "SWING", which is
// no use to read off a wall.
const char* wideVaneName(WideVaneMode wideVane) {
  switch (wideVane) {
    case WideVaneMode::FarLeft:  return "Far left";
    case WideVaneMode::Left:     return "Left";
    case WideVaneMode::Center:   return "Center";
    case WideVaneMode::Right:    return "Right";
    case WideVaneMode::FarRight: return "Far right";
    case WideVaneMode::Spread:   return "Spread both ways";
    case WideVaneMode::Swing:    return "Swing";
  }
  return "?";
}

bool usesTargetTemp(HvacMode mode) {
  // Dry has no setpoint of its own: the unit decides when to dehumidify
  // ("Temperature cannot be set during DRY mode" in Mitsubishi's manual).
  // The unit still reports a temperature — the setpoint Heat/Cool will use.
  return mode != HvacMode::Off && mode != HvacMode::FanOnly && mode != HvacMode::Dry;
}
