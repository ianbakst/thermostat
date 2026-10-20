#include "climate/temperature.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#define DEG "\xC2\xB0"  // UTF-8 degree sign (included in LVGL's Montserrat fonts)

float stepTargetTemp(float target, int direction, float step, float min, float max) {
  if (std::isnan(target) || std::isnan(min) || std::isnan(max) || !(step > 0)) return target;

  // Work in whole steps. Limits round inward, so the setpoint never passes
  // them. Starting from the nearest step matches what's displayed: 70.6°
  // shows as 71°, so + goes to 72°.
  const float lowest = std::ceil(min / step - 0.01f);
  const float highest = std::floor(max / step + 0.01f);
  if (lowest > highest) return target;
  const float next = std::clamp(std::round(target / step) + direction, lowest, highest) * step;

  // Already at the limit: leave the setpoint untouched.
  if ((direction > 0 && next <= target) || (direction < 0 && next >= target)) return target;
  return next;
}

void formatTemp(char* buf, size_t len, float temp, TempUnit unit, float step) {
  if (std::isnan(temp)) {
    snprintf(buf, len, "--" DEG);
    return;
  }
  // A decimal only if the unit works in fractions of a degree.
  const int decimals = step == std::floor(step) ? 0 : 1;
  snprintf(buf, len, "%.*f" DEG "%s", decimals, temp, unit == TempUnit::Fahrenheit ? "F" : "C");
}
