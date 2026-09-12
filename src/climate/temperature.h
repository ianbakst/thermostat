#pragma once
//
// Setpoint stepping and temperature formatting. Temperatures are in whatever
// unit the mini-split uses; nothing here converts between °F and °C.
//

#include <cstddef>
#include <cstdint>

enum class TempUnit : uint8_t { Fahrenheit, Celsius };

// One +/- tap: moves the setpoint one `step` up (direction +1) or down (-1),
// staying within min..max. Returns `target` unchanged if it's already at the
// limit or anything is unknown (NAN).
float stepTargetTemp(float target, int direction, float step, float min, float max);

// Writes e.g. "72°F", or "21.5°C" when the step isn't a whole degree.
// Unknown (NAN) temperatures show as "--°".
void formatTemp(char* buf, size_t len, float temp, TempUnit unit, float step);
