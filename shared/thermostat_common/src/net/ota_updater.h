#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <atomic>

#include "net/ota_status.h"

// Over-the-air firmware updates from this repo's GitHub releases, shared by
// both firmwares. Each has its own manifest (panel-manifest.json,
// unit-manifest.json), so neither is ever offered the other's image.
//
// Two steps, both run in a FreeRTOS task of their own so neither stalls the
// caller — the panel's UI, the controller's heat pump link (the same
// arrangement as net/mqtt_connection.h):
//
//  1. Check: fetch the channel's small manifest and compare its version with
//     the one running.
//  2. Install: stream the firmware it points at into whichever app slot isn't
//     running. The caller then restarts into it.
//
// Both connect over TLS, verified against the certificate bundle built into
// the ESP-IDF (`crt_bundle_attach`), so there's no CA certificate here to
// expire. The image itself carries a SHA-256 that esp_ota checks before the
// device is allowed to boot from it, so a truncated or corrupted download is
// refused rather than installed. Nothing is ever installed except what a
// manifest on GitHub points at.
//
// After an update the bootloader keeps the previous firmware in reserve until
// the new one proves itself (CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE, on in the
// Arduino core). tick() is what confirms it, once the new firmware has run
// for `selfTestMs` with the network up. Until then a firmware that crashes or
// can't reach the network is replaced by the one it came from at the next
// boot, with no USB cable involved — and so is one that's simply restarted
// before then.
class OtaUpdater {
 public:
  // Where to look and how patiently, which each firmware sets from its own
  // config.h. The strings must stay valid for as long as the updater runs.
  struct Options {
    const char* stableManifestUrl = nullptr;  // the newest tagged release's manifest
    const char* betaManifestUrl = nullptr;    // the rolling prerelease's
    const char* firmwareVersion = "dev";      // what's running, from FIRMWARE_VERSION
    unsigned long checkIntervalMs = 24UL * 60 * 60 * 1'000;
    unsigned long firstCheckDelayMs = 60'000;  // after the network has settled
    unsigned long selfTestMs = 60'000;        // a new image's trial run
    int httpTimeoutMs = 15'000;
    // A release asset URL is answered with a redirect to GitHub's storage
    // host, and the signed URL in that Location header — then in the request
    // that follows it — runs to about a kilobyte. esp_http_client defaults
    // both buffers to 512 bytes, which the redirect doesn't fit in: it gives
    // up with "Out of buffer" rather than anything that names the real
    // problem. These apply to the download as well, which takes the same road.
    int httpRxBuffer = 4'096;
    int httpTxBuffer = 2'048;
    // The task only exists while a check or a download runs. Its stack (in
    // bytes, as ESP-IDF counts it) has to hold a TLS handshake against the
    // certificate bundle, which is most of it.
    uint32_t taskStack = 12'288;
    unsigned taskPriority = 1;  // the Arduino loop's, so neither starves the other
  };

  void begin(OtaChannel channel, const Options& options);

  OtaChannel channel() const { return channel_.load(); }

  // Which releases to look at. Changing it clears what the last check found.
  void setChannel(OtaChannel channel);

  // Ask now, from the settings page's button. One asked for while a check is
  // already running waits for it and runs next, so a check straight after a
  // change of channel isn't lost. Ignored while a download is running.
  // `nowMs` is the loop's clock for this pass (see main.cpp): every check,
  // however it was asked for, pushes the next automatic one a full interval
  // out, so a manual check can't be chased by a scheduled one seconds later.
  void check(unsigned long nowMs);

  // Download and install the version the last check found. Ignored unless
  // the state is Available.
  void install();

  // Call from loop(). Runs the first check shortly after the network comes up
  // and one every `checkIntervalMs` after that, and confirms this image once
  // it has proved itself. Returns true when the status changed, i.e. when
  // there's something new to show. It never installs anything by itself.
  bool tick(unsigned long nowMs, bool networkReady);

  OtaStatus status() const;

  // True while the firmware is being written. The panel keeps its screen on
  // then, to show how far it's got. A check doesn't count: it's a background
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
  OtaStatus reported_;  // what tick() last handed to the caller

  Options options_;
  char userAgent_[48] = "";
  Job job_ = Job::Check;
  // Set by loop(), read by the check job, which drops its answer if the
  // channel changed while it was out asking.
  std::atomic<OtaChannel> channel_{OtaChannel::Stable};
  bool checkPending_ = false;  // asked for while a check was running

  // Written by the check job, read by the install job that follows it. Only
  // one job ever runs at a time, so they can't overlap.
  char downloadUrl_[192] = "";

  std::atomic<bool> running_{false};  // cleared by the task as it exits
  unsigned long lastCheckMs_ = 0;
  bool checkedOnce_ = false;
  bool imageConfirmed_ = false;
};
