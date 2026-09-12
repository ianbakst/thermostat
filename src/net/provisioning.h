#pragma once
#include <DNSServer.h>
#include <WebServer.h>

#include "climate/mini_split.h"
#include "net/mqtt_connection.h"
#include "storage/settings_store.h"

// Setup mode: the panel runs its own Wi-Fi network with a three-step setup
// page on it, so no network name, password or broker login is ever compiled
// into the firmware.
//
//   1. Wi-Fi       the network, its password, and a name for the panel.
//                  "Next" connects, and only moves on if that works.
//   2. MQTT        broker, port, username, password. "Next" connects to the
//                  broker, and only moves on if that works.
//   3. Mini-split  pick one of the units Home Assistant advertises — which
//                  the panel only knows because step 2 succeeded.
//
// Each step tests what it collected instead of saving and hoping, so a typo is
// caught on the page where it was made and named there ("the broker refused
// that login", not a silent retry).
//
// The network's name and a password generated fresh each time are shown on the
// panel with a QR code a phone can scan to join. Once joined, the page opens
// by itself: phones check for a captive portal when they join a network, and
// the DNS server here answers every lookup with the panel's own address.
//
// Finishing saves everything and restarts the panel; main.cpp does both.
class Provisioning {
 public:
  using OnFinished = void (*)(void* context, const DeviceSettings& settings);

  enum class Mode : uint8_t {
    // First run, or "Wi-Fi setup": the panel's own network, starting at step 1.
    Full,
    // "MQTT setup": the network is already working, so there's no need for the
    // panel's own one. The pages are served over the network the panel is on,
    // starting at step 2, and the panel shows its address as a QR code. The
    // phone never has to leave your Wi-Fi.
    BrokerOnly,
  };

  // The broker connection and the mini-split are borrowed to test step 2 and
  // to list the units in step 3.
  Provisioning(MqttConnection& mqtt, MiniSplit& miniSplit) : mqtt_(mqtt), miniSplit_(miniSplit) {}

  // `current` pre-fills the form; passwords are never sent to the browser.
  void begin(Mode mode, const DeviceSettings& current, OnFinished onFinished, void* context);

  Mode mode() const { return mode_; }

  // Where the setup pages are, e.g. "http://192.168.4.1/".
  const char* pageUrl() const { return pageUrl_; }

  // Stops the network and the page, and returns the radio to normal use.
  void end();

  bool active() const { return active_; }

  // Call from loop() while active(): serves the page and watches each test.
  void tick(unsigned long nowMs);

  const char* apSsid() const { return apSsid_; }
  const char* apPassword() const { return apPassword_; }

  // What a phone scans to join: "WIFI:T:WPA;S:<network>;P:<password>;;".
  const char* joinQrData() const { return joinQr_; }

  // Where the setup has got to, for the panel's own screen. Always one of a
  // few fixed strings, so callers can compare the pointer to spot a change.
  const char* progress() const;

 private:
  enum class Step : uint8_t { Wifi, WifiTesting, Mqtt, MqttTesting, Unit, Done };

  void sendCurrentPage();
  void redirectToCurrentPage();
  void submitWifi();
  void submitMqtt();
  void submitUnit();
  void startWifiTest();
  void startMqttTest();
  void failStep(Step step, const char* error);
  const char* stepLabel() const;
  int stepCount() const { return mode_ == Mode::Full ? 3 : 2; }

  WebServer server_{80};
  DNSServer dns_;
  MqttConnection& mqtt_;
  MiniSplit& miniSplit_;

  DeviceSettings draft_;  // what's been entered so far; saved only at the end
  OnFinished onFinished_ = nullptr;
  void* context_ = nullptr;
  bool active_ = false;

  Mode mode_ = Mode::Full;
  Step step_ = Step::Wifi;
  // This tick's time, used by the page handlers too: they run inside tick(),
  // and a test started from one must not look as though it began in the
  // future (unsigned subtraction would then make it instantly overdue).
  unsigned long nowMs_ = 0;
  unsigned long testStartedMs_ = 0;
  const char* error_ = nullptr;  // why the last test failed, shown on its page

  char apSsid_[33] = "";
  char apPassword_[SETUP_AP_PASSWORD_LEN + 1] = "";
  char joinQr_[80] = "";
  char pageUrl_[48] = "";
  String networkOptions_;  // nearby networks, from the scan when setup started
};
