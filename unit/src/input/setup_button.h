#pragma once

// The XIAO's BOOT button, which puts a unit that's already set up back into
// setup mode when held (see net/provisioning.h). It's the way back for a unit
// that can't be reached on the network to use its page: the Wi-Fi password
// changed, or the page was given settings that don't work.
//
// It counts only while the controller is running, not at power-up. See
// SETUP_BUTTON_PIN in config.h for why.
class SetupButton {
 public:
  void begin();

  // Call from loop(). Returns true once the button has been held for
  // SETUP_BUTTON_HOLD_MS, and not again until it's let go.
  bool tick(unsigned long nowMs);

 private:
  bool down_ = false;
  bool fired_ = false;
  unsigned long downSinceMs_ = 0;
};
