#include "net/wifi_connection.h"

#include <ESPmDNS.h>
#include <WiFi.h>

#include <cstring>

#include "config.h"

void WifiConnection::begin(const char* ssid, const char* password, const char* hostname) {
  ssid_ = ssid;
  password_ = password;
  hostname_ = hostname;
  started_ = true;

  WiFi.mode(WIFI_STA);            // join a network (not act as an access point)
  WiFi.setHostname(hostname_);    // must be set before connecting
  WiFi.setAutoReconnect(true);
  connect(millis());
}

void WifiConnection::connect(unsigned long nowMs) {
  Serial.printf("Wi-Fi: connecting to \"%s\"\n", ssid_);
  WiFi.begin(ssid_, password_);
  lastAttemptMs_ = nowMs;
}

bool WifiConnection::tick(unsigned long nowMs) {
  if (!started_) return false;

  const bool connected = WiFi.status() == WL_CONNECTED;
  const bool changed = connected != connected_;
  connected_ = connected;

  if (changed && connected) {
    Serial.printf("Wi-Fi: connected, IP %s, signal %d dBm, free heap %u bytes\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI(),
                  static_cast<unsigned>(ESP.getFreeHeap()));
    // Answer to <panel name>.local, so the panel can be found without
    // hunting for its IP address.
    if (!mdnsStarted_ && MDNS.begin(hostname_)) {
      mdnsStarted_ = true;
      Serial.printf("Wi-Fi: also reachable as %s.local\n", hostname_);
    }
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
  strncpy(status.ssid, ssid_, sizeof(status.ssid) - 1);
  if (connected_) {
    strncpy(status.ip, WiFi.localIP().toString().c_str(), sizeof(status.ip) - 1);
    status.rssi = static_cast<int8_t>(WiFi.RSSI());
  }
  return status;
}
