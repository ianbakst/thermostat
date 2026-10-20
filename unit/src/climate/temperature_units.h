#pragma once
//
// The heat pump works in °C. Everything above it — this controller's page,
// the panels, Home Assistant — sees the unit chosen during setup instead, °F
// here, so temperatures are converted on the way out and on the way in.
//
// °F is whole degrees, as the remote shows it and mitsubishi2mqtt published
// it. On a unit that takes half degrees, the round trip is exact for every
// setpoint it accepts (61-88°F): 73°F goes in as 22.8°C, the unit sets 23,
// and 23°C comes back out as 73°F. Older units take whole degrees only, and
// there a setpoint can come back 1°F off what was asked for.
//

#include <cmath>

// From the heat pump's °C to the unit everything else uses. Unknown (NAN)
// stays unknown.
inline float toReported(float celsius, bool fahrenheit) {
  if (!fahrenheit || std::isnan(celsius)) return celsius;
  return std::round(celsius * 1.8f + 32);
}

// The other way, for a setpoint asked for in the reported unit. HeatPumpLink
// rounds the result to what the unit can actually set.
inline float toCelsius(float reported, bool fahrenheit) {
  return fahrenheit ? (reported - 32) / 1.8f : reported;
}
