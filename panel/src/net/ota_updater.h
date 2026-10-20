#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <atomic>

#include "net/ota_status.h"

// Over-the-air firmware updates, from the GitHub releases of this repo.
//
// Two steps, both run in a FreeRTOS task of their own so neither stalls the
// UI (the same arrangement as net/mqtt_connection.h):
//
//  1. Check: fetch the channel's small manifest.json (OTA_STABLE_MANIFEST_URL
//     / OTA_BETA_MANIFEST_URL) and compare its version to FIRMWARE_VERSION.
//  2. Install: stream the firmware.bin it points at into whichever app slot
//     isn't running, then restart into it.
//
// Both connect over TLS, verified against the certificate bundle built into
// the ESP-IDF (`crt_bundle_attach`), so there's no CA certificate here to
// expire. The image itself carries a SHA-256 that esp_ota checks before the
// panel is allowed to boot from it, so a truncated or corrupted download is
// refused rather than installed.
//
// After an update the bootloader keeps the previous firmware in reserve until
// the new one proves itself (CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE, on in the
// Arduino core). tick() is what confirms it: see OTA_SELF_TEST_MS. Until then
// a firmware that crashes or can't reach the network is replaced by the one it
// came from on the next boot, with no USB cable involved.
class OtaUpdater {
 public:
  void begin(OtaChannel channel);

  // Which releases to look at. Changing it clears what the last check found.
  void setChannel(OtaChannel channel);

  // Ask now, from the settings page's button. Ignored while a check or a
  // download is already running. `nowMs` is the loop's clock for this pass
  // (see main.cpp): every check, however it was asked for, pushes the next
  // automatic one a full interval out, so a manual check can't be chased by a
  // scheduled one seconds later.
  void check(unsigned long nowMs);

  // Download and install the version the last check found. Ignored unless
  // the state is Available.
  void install();

  // Call from loop(). Runs the first check shortly after the network comes up
  // and one every OTA_CHECK_INTERVAL_MS after that, and confirms this image
  // once it has proved itself. Returns true when the status changed, i.e.
  // when there's something new to show.
  bool tick(unsigned long nowMs, bool networkReady);

  OtaStatus status() const;

  // True while the firmware is being written, which is when the panel stays
  // awake to show how far it's got. A check doesn't count: it's a background
  // errand, and lighting the screen for it would wake a dark room once a day.
  bool installing() const;

 private:
  enum class Job : uint8_t { Check, Install };

  static void runJob(void* self);
  void doCheck();
  void doInstall();

  // Both of these take lock_. publish() sets a terminal state and the text
  // that goes with it; setProgress() only moves the bar along.
  void publish(OtaState state, const char* version, const char* message);
  void setProgress(int percent);

  bool start(Job job);  // creates the task if nothing else is running

  SemaphoreHandle_t lock_ = nullptr;  // guards status_ between the task and loop()
  OtaStatus status_;
  OtaStatus reported_;  // what tick() last handed to the UI

  Job job_ = Job::Check;
  OtaChannel channel_ = OtaChannel::Stable;

  // Written by the check job, read by the install job that follows it. Only
  // one job ever runs at a time, so they can't overlap.
  char downloadUrl_[192] = "";

  std::atomic<bool> running_{false};  // cleared by the task as it exits
  unsigned long lastCheckMs_ = 0;
  bool checkedOnce_ = false;
  bool imageConfirmed_ = false;
};
