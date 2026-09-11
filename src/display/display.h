#pragma once
#include <cstdint>

// Owns the screen: brings up the LCD panel and touch controller and hooks
// them into LVGL as a display and a pointer input. After begin(), everything
// visual goes through LVGL (see ui/).
class Display {
 public:
  void begin();

  // Runs LVGL timers, rendering and touch input. Call every loop iteration.
  void tick();

  void setBrightness(uint8_t level);  // 0-255
};
