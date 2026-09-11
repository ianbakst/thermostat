#pragma once
#include <cstdint>

// Driver for the AXS5106L capacitive touch controller on I2C.
//
// Reports a single touch point in *screen* coordinates: the raw panel
// coordinates are rotated to match DISPLAY_ROTATION so (0, 0) is always the
// top-left of what you see.
//
// The controller pulls its INT pin low whenever it has a new touch report.
// We only talk to it over I2C after that happens: when nobody has touched it
// for a while it goes to sleep and stops answering I2C, so polling it
// constantly just produces errors.
class Axs5106l {
 public:
  // Resets the controller and checks it answers on I2C. Returns false if it
  // doesn't (the rest of the firmware still runs, just without touch).
  bool begin(uint8_t rotation);

  // Call often (LVGL does, every 16ms). Returns true and fills x/y while a
  // finger is down.
  bool read(int32_t& x, int32_t& y);

  // TEMPORARY DIAGNOSTIC: tries an I2C read without waiting for INT and logs
  // whether the controller answered.
  void probe();

 private:
  bool readRegister(uint8_t reg, uint8_t* data, uint8_t len);

  uint8_t rotation_ = 0;
  bool pressed_ = false;
  int32_t x_ = 0;
  int32_t y_ = 0;
  unsigned long lastReportMs_ = 0;
  unsigned long lastErrorLogMs_ = 0;

  // DIAG
  unsigned long diagDownMs_ = 0;
  uint32_t diagReports_ = 0;
};
