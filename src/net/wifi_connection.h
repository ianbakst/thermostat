#pragma once

#include "net/wifi_status.h"

// Keeps the panel connected to Wi-Fi, using the network in include/secrets.h.
//
// Connecting happens in the background (the Wi-Fi driver runs in its own
// tasks), so begin() returns immediately and the UI keeps running.
//
// The Arduino core reconnects by itself after most drops. It gives up after
// some failures, though (notably AUTH_FAIL, which some routers report briefly
// after they reboot), so as a safety net tick() starts a fresh attempt
// whenever we've been offline for WIFI_RETRY_INTERVAL_MS.
class WifiConnection {
 public:
  void begin();

  // Call from loop(). Returns true when we've just connected or disconnected.
  bool tick(unsigned long nowMs);

  WifiStatus status() const;

 private:
  void connect(unsigned long nowMs);

  bool connected_ = false;
  unsigned long lastAttemptMs_ = 0;
};
