#pragma once
//
// Temperature units: conversion, setpoint stepping and formatting.
//

#include <cstddef>
#include <cstdint>

enum class TempUnit : uint8_t { Fahrenheit, Celsius };

float fahrenheitToCelsius(float f);
float celsiusToFahrenheit(float c);

// Converts between stored °C and the unit shown on screen.
float toDisplayUnit(float celsius, TempUnit unit);
float fromDisplayUnit(float value, TempUnit unit);

// One +/- tap: moves the setpoint by 1°F or 0.5°C (direction is +1 or -1),
// snapped to that step so it always lands on a round number in the display
// unit, and clamped to TARGET_TEMP_MIN_C..TARGET_TEMP_MAX_C.
float stepTargetTemp(float targetC, int direction, TempUnit unit);

// Writes e.g. "72°" (°F, whole degrees) or "21.5°" (°C, one decimal).
// Unknown (NAN) temperatures show as "--°".
void formatTemp(char* buf, size_t len, float celsius, TempUnit unit);
