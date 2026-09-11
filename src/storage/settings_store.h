#pragma once
#include <Preferences.h>

#include "climate/temperature.h"

// Device settings saved in the ESP32's flash (NVS), so they survive reboots
// and power cuts. These are the panel's own settings; the mini-split's mode
// and setpoint belong to Home Assistant and aren't stored here.
class SettingsStore {
 public:
  void begin();

  TempUnit loadTempUnit(TempUnit fallback);
  void saveTempUnit(TempUnit unit);

 private:
  Preferences prefs_;
};
