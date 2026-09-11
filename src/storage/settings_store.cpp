#include "storage/settings_store.h"

// NVS groups keys by namespace; keys are limited to 15 characters.
static const char* NAMESPACE = "thermostat";
static const char* KEY_TEMP_UNIT = "temp_unit";

void SettingsStore::begin() { prefs_.begin(NAMESPACE, false /* read-write */); }

TempUnit SettingsStore::loadTempUnit(TempUnit fallback) {
  const uint8_t value = prefs_.getUChar(KEY_TEMP_UNIT, static_cast<uint8_t>(fallback));
  // Ignore anything unexpected (e.g. from a future firmware) rather than
  // trusting it.
  if (value > static_cast<uint8_t>(TempUnit::Celsius)) return fallback;
  return static_cast<TempUnit>(value);
}

void SettingsStore::saveTempUnit(TempUnit unit) {
  prefs_.putUChar(KEY_TEMP_UNIT, static_cast<uint8_t>(unit));
}
