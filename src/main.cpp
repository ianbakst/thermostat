// thermostat — in-wall touch panel for mini-split heat pumps
//
// Hardware: Waveshare ESP32-C6-Touch-LCD-1.47.
//
// This file only wires modules together and runs the loop. Each piece of
// hardware has its own module under src/.

#include <Arduino.h>

#include "climate/climate.h"
#include "climate/temperature.h"
#include "config.h"
#include "display/display.h"
#include "net/wifi_connection.h"
#include "storage/settings_store.h"
#include "ui/ui.h"

static Display display;
static Ui ui;
static SettingsStore settingsStore;
static WifiConnection wifi;

// The panel's own settings, loaded from flash at startup.
static TempUnit tempUnit = DEFAULT_TEMP_UNIT;

// The mini-split's state. Placeholder values for now; from step 4 this is
// filled in from Home Assistant.
static ClimateState climate;

// ---------------------------------------------------------------------------
// What happens when the user taps a control. For now: apply it locally and
// redraw. In step 4 the climate ones send commands to Home Assistant instead.
// ---------------------------------------------------------------------------

static void setTargetTemp(float celsius) {
  climate.targetTempC = celsius;
  char buf[16];
  formatTemp(buf, sizeof(buf), celsius, tempUnit, true /* showUnit */);
  Serial.printf("Requested target: %s\n", buf);
  ui.show(climate);
}

static void setMode(HvacMode mode) {
  climate.mode = mode;
  Serial.printf("Requested mode: %s\n", modeName(mode));
  ui.show(climate);
}

static void setFanMode(FanMode fan) {
  climate.fan = fan;
  Serial.printf("Requested fan: %s\n", fanModeName(fan));
  ui.show(climate);
}

static void setTempUnit(TempUnit unit) {
  tempUnit = unit;
  settingsStore.saveTempUnit(unit);
  Serial.printf("Units: %s (saved)\n", tempUnitSymbol(unit));
  ui.showTempUnit(unit);

  // Snap the setpoint to a round number in the new unit (e.g. 70°F is
  // 21.11°C, which becomes 21.0°C). This changes the setpoint, so it goes
  // through setTargetTemp like an arrow tap would.
  const float rounded = roundTargetTemp(climate.targetTempC, unit);
  if (!std::isnan(rounded) && rounded != climate.targetTempC) setTargetTemp(rounded);
}

// ---------------------------------------------------------------------------

void setup() {
  // Serial runs over the C6's built-in USB. When a computer is attached but
  // nothing is reading the port (no serial monitor open), each write would
  // wait up to ~2s for it by default, freezing the UI on every log line.
  // With no timeout, output is dropped instead when nobody is listening; the
  // larger buffer keeps bursts like the startup log intact when someone is.
  Serial.setTxBufferSize(1024);
  Serial.setTxTimeoutMs(0);
  Serial.begin(115200);
  Serial.println();
  Serial.println("Thermostat starting");
  Serial.println("-------------------------------------");

  settingsStore.begin();
  tempUnit = settingsStore.loadTempUnit(DEFAULT_TEMP_UNIT);
  Serial.printf("Units: %s\n", tempUnitSymbol(tempUnit));

  climate.currentTempC = fahrenheitToCelsius(71.0f);
  climate.targetTempC = fahrenheitToCelsius(70.0f);
  climate.mode = HvacMode::Heat;
  climate.fan = FanMode::Auto;

  display.begin();
  ui.begin(tempUnit, {setTargetTemp, setMode, setFanMode, setTempUnit});
  ui.show(climate);

  // Connects in the background; the UI is already up and responsive.
  wifi.begin();
}

void loop() {
  const unsigned long now = millis();

  // Refresh the Wi-Fi status on the settings page when the connection comes
  // or goes, and every few seconds so the signal strength stays current.
  static unsigned long lastWifiStatusMs = 0;
  if (wifi.tick(now) || now - lastWifiStatusMs >= WIFI_STATUS_REFRESH_MS) {
    lastWifiStatusMs = now;
    ui.showWifiStatus(wifi.status());
  }

  display.tick();
  delay(LVGL_LOOP_DELAY_MS);
}
