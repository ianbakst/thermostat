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
#include "ui/ui.h"

static Display display;
static Ui ui;

// The mini-split's state. Placeholder values for now; from step 4 this is
// filled in from Home Assistant.
static ClimateState climate;

// ---------------------------------------------------------------------------
// What happens when the user taps a control. For now: apply it locally and
// redraw. In step 4 these send commands to Home Assistant instead.
// ---------------------------------------------------------------------------

static void setTargetTemp(float celsius) {
  climate.targetTempC = celsius;
  char buf[16];
  formatTemp(buf, sizeof(buf), celsius, DEFAULT_TEMP_UNIT);
  Serial.printf("Requested target: %s\n", buf);
  ui.show(climate);
}

static void setMode(HvacMode mode) {
  climate.mode = mode;
  Serial.printf("Requested mode: %s\n", modeName(mode));
  ui.show(climate);
}

static void setFanSpeed(FanSpeed fan) {
  climate.fan = fan;
  Serial.printf("Requested fan: %s\n", fanSpeedName(fan));
  ui.show(climate);
}

// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Thermostat starting");
  Serial.println("-------------------------------------");

  climate.currentTempC = fahrenheitToCelsius(71.0f);
  climate.targetTempC = fahrenheitToCelsius(70.0f);
  climate.mode = HvacMode::Heat;
  climate.fan = FanSpeed::Auto;

  display.begin();
  ui.begin(DEFAULT_CLIMATE_NAME, DEFAULT_TEMP_UNIT, {setTargetTemp, setMode, setFanSpeed});
  ui.show(climate);
}

void loop() {
  display.tick();
  delay(LVGL_LOOP_DELAY_MS);
}
