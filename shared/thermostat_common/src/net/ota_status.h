#pragma once
#include <cstdint>

// A snapshot of where the firmware update stands, for display. Plain data with
// no Arduino or ESP-IDF types, so the panel's UI or the controller's page can
// show it without depending on the update code (the same arrangement as
// net/wifi_status.h).

// Which releases a device is offered. Saved in flash; picked on its settings
// page. See the release workflow in .github/workflows/.
enum class OtaChannel : uint8_t {
  Stable = 0,  // tagged releases off main
  Beta = 1,    // the rolling prerelease rebuilt on every push to dev
};

enum class OtaState : uint8_t {
  Idle,         // nothing asked for yet this boot
  Checking,     // fetching the channel's manifest
  UpToDate,     // the release matches what's running
  Available,    // there's a different version to install
  Downloading,  // writing it into the spare app slot
  Installed,    // written and verified; the device restarts into it
  Failed,       // see `message`
};

struct OtaStatus {
  OtaState state = OtaState::Idle;
  int progress = 0;       // 0-100 while Downloading, -1 if the size is unknown
  char version[32] = "";  // the version on offer, once one is known
  char message[72] = "";  // why the last attempt failed
};

inline const char* channelName(OtaChannel channel) {
  return channel == OtaChannel::Beta ? "beta" : "stable";
}
