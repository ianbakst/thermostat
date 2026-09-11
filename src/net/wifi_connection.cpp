#include "net/wifi_connection.h"

#include <WiFi.h>

#include <cstring>

#include "config.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Missing include/secrets.h. Copy include/secrets.example.h to include/secrets.h and fill in your Wi-Fi network name and password."
#endif

void WifiConnection::begin() {
  WiFi.mode(WIFI_STA);              // join a network (not act as an access point)
  WiFi.setHostname(WIFI_HOSTNAME);  // must be set before connecting
  WiFi.setAutoReconnect(true);
  connect(millis());
}

void WifiConnection::connect(unsigned long nowMs) {
  Serial.printf("Wi-Fi: connecting to \"%s\"\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastAttemptMs_ = nowMs;
}

bool WifiConnection::tick(unsigned long nowMs) {
  const bool connected = WiFi.status() == WL_CONNECTED;
  const bool changed = connected != connected_;
  connected_ = connected;

  if (changed && connected) {
    Serial.printf("Wi-Fi: connected, IP %s, signal %d dBm, free heap %u bytes\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI(), ESP.getFreeHeap());
  } else if (changed) {
    // The core logs the reason just before this ("Reason: ..."). Give its
    // automatic reconnect a full interval before we step in.
    Serial.println("Wi-Fi: disconnected");
    lastAttemptMs_ = nowMs;
  }

  if (!connected && nowMs - lastAttemptMs_ >= WIFI_RETRY_INTERVAL_MS) {
    Serial.println("Wi-Fi: still offline, starting a new attempt");
    WiFi.disconnect();
    connect(nowMs);
  }

  return changed;
}

WifiStatus WifiConnection::status() const {
  WifiStatus status;
  status.connected = connected_;
  strncpy(status.ssid, WIFI_SSID, sizeof(status.ssid) - 1);
  if (connected_) {
    strncpy(status.ip, WiFi.localIP().toString().c_str(), sizeof(status.ip) - 1);
    status.rssi = static_cast<int8_t>(WiFi.RSSI());
  }
  return status;
}
