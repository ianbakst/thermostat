#pragma once

#include "net/wifi_status.h"

// Keeps a device connected to the network saved in flash (entered on its
// setup page). Shared by both firmwares.
//
// Connecting happens in the background (the Wi-Fi driver runs in its own
// tasks), so begin() returns immediately and the caller keeps running: the
// panel's UI stays live, and the mini-split keeps talking to its heat pump.
//
// The Arduino core reconnects by itself after most drops. It gives up after
// some failures, though (notably AUTH_FAIL, which some routers report briefly
// after they reboot), so as a safety net tick() starts a fresh attempt
// whenever we've been offline for `retryIntervalMs`.
class WifiConnection {
 public:
  // The strings must stay valid: they're the saved settings, which live for
  // as long as the program does. `retryIntervalMs` is how long offline before
  // a fresh attempt; each firmware sets it in its own config.h.
  void begin(const char* ssid, const char* password, const char* hostname,
             unsigned long retryIntervalMs);

  // Call from loop(). Returns true when we've just connected or disconnected.
  bool tick(unsigned long nowMs);

  // Stops managing the connection, for when setup mode takes over the radio to
  // try a different network. Otherwise the retry here would fight it, pulling
  // the device back to the saved network mid-test. The device restarts when
  // setup finishes or is cancelled, so there's no resume.
  void pause() { started_ = false; }

  WifiStatus status() const;
  bool connected() const { return connected_; }

 private:
  void connect(unsigned long nowMs);

  const char* ssid_ = "";
  const char* password_ = "";
  const char* hostname_ = "";
  unsigned long retryIntervalMs_ = 30'000;
  bool started_ = false;
  bool connected_ = false;
  bool mdnsStarted_ = false;
  unsigned long lastAttemptMs_ = 0;
};
