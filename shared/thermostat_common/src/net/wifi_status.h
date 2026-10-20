#pragma once
#include <cstdint>

// A snapshot of the Wi-Fi connection, for display. Plain data with no
// Arduino types, so the UI can show it without depending on the Wi-Fi code.
struct WifiStatus {
  bool connected = false;
  char ssid[33] = "";  // the network we're connecting / connected to
  char ip[16] = "";    // e.g. "192.168.1.40"; empty when not connected
  int8_t rssi = 0;     // signal strength in dBm (e.g. -55); 0 when not connected
};

// Signal strength as 1-4 bars, like a phone's Wi-Fi icon. Used for both the
// icon and the settings page's description, so the two always agree.
inline int signalBars(int rssi) {
  if (rssi >= -60) return 4;  // strong
  if (rssi >= -70) return 3;  // good
  if (rssi >= -80) return 2;  // weak
  return 1;                   // very weak
}
