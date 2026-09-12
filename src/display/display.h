#pragma once
#include <cstdint>

// Owns the screen: brings up the LCD panel and touch controller and hooks
// them into LVGL as a display and a pointer input. After begin(), everything
// visual goes through LVGL (see ui/).
//
// Also handles sleep: the backlight turns off after DISPLAY_SLEEP_TIMEOUT_MS
// without a touch, and the next touch turns it back on without pressing
// anything.
class Display {
 public:
  void begin();

  // Runs LVGL timers, rendering and touch input, and puts the display to
  // sleep when it's been idle. Call every loop iteration.
  void tick();

  // Backlight level while awake, 0-255.
  void setBrightness(uint8_t level);

  // Allows or blocks the sleep timeout. Blocking it also wakes the display,
  // and is for screens that have to stay readable without being touched (the
  // setup screen, whose QR code gets scanned).
  void setSleepAllowed(bool allowed);
};
