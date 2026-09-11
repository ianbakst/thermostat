#include "climate/temperature.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "config.h"

#define DEG "\xC2\xB0"  // UTF-8 degree sign (included in LVGL's Montserrat fonts)

float fahrenheitToCelsius(float f) { return (f - 32.0f) * 5.0f / 9.0f; }
float celsiusToFahrenheit(float c) { return c * 9.0f / 5.0f + 32.0f; }

float toDisplayUnit(float celsius, TempUnit unit) {
  return unit == TempUnit::Fahrenheit ? celsiusToFahrenheit(celsius) : celsius;
}

float fromDisplayUnit(float value, TempUnit unit) {
  return unit == TempUnit::Fahrenheit ? fahrenheitToCelsius(value) : value;
}

// Smallest change the screen shows. Must match the formats in formatTemp().
static float displayPrecision(TempUnit unit) {
  return unit == TempUnit::Fahrenheit ? 1.0f : 0.1f;
}

float stepTargetTemp(float targetC, int direction, TempUnit unit) {
  const float step = unit == TempUnit::Fahrenheit ? 1.0f : 0.5f;
  const float precision = displayPrecision(unit);

  // Work in steps of the display unit, starting from the value as displayed
  // (70.7°F shows as 71°, so + must go to 72°, not 71°).
  const float shown = std::round(toDisplayUnit(targetC, unit) / precision) * precision;
  const float steps = shown / step;
  const float nearest = std::round(steps);

  float next;
  if (std::fabs(steps - nearest) < 0.01f) {
    // On a step (allowing for float error): move one step.
    next = nearest + direction;
  } else {
    // Between steps (e.g. 21.3°C): move to the next step in that direction.
    next = direction > 0 ? std::ceil(steps) : std::floor(steps);
  }

  // Limits rounded inward to whole steps: 16°C is 60.8°F, so the lowest °F
  // setpoint is 61°.
  const float lo = std::ceil(toDisplayUnit(TARGET_TEMP_MIN_C, unit) / step - 0.01f);
  const float hi = std::floor(toDisplayUnit(TARGET_TEMP_MAX_C, unit) / step + 0.01f);
  next = std::clamp(next, lo, hi);

  // Already at the limit: leave the setpoint untouched.
  if ((direction > 0 && next <= steps) || (direction < 0 && next >= steps)) return targetC;

  return fromDisplayUnit(next * step, unit);
}

void formatTemp(char* buf, size_t len, float celsius, TempUnit unit) {
  if (std::isnan(celsius)) {
    snprintf(buf, len, "--" DEG);
  } else if (unit == TempUnit::Fahrenheit) {
    snprintf(buf, len, "%ld" DEG, std::lround(celsiusToFahrenheit(celsius)));
  } else {
    snprintf(buf, len, "%.1f" DEG, celsius);
  }
}
