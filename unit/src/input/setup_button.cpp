#include "input/setup_button.h"

#include <Arduino.h>

#include "config.h"

void SetupButton::begin() { pinMode(SETUP_BUTTON_PIN, INPUT_PULLUP); }

bool SetupButton::tick(unsigned long nowMs) {
  if (digitalRead(SETUP_BUTTON_PIN) == HIGH) {
    down_ = false;
    return false;
  }

  if (!down_) {
    down_ = true;
    fired_ = false;
    downSinceMs_ = nowMs;
    // There's no LED to show it, so the log is the only sign it was seen.
    Serial.printf("Setup button: held, setup mode in %lu seconds unless let go\n",
                  SETUP_BUTTON_HOLD_MS / 1'000);
    return false;
  }

  if (fired_ || nowMs - downSinceMs_ < SETUP_BUTTON_HOLD_MS) return false;
  fired_ = true;
  return true;
}
