#include "net/provisioning.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_attr.h>

#include "config.h"
#include "net/settings_form.h"

// Set by the BOOT button just before it restarts the controller. This memory
// survives a restart but not a power cut, and nothing clears it at boot, so it
// starts out as whatever happened to be there: hence a value that won't turn
// up by chance, rather than a bool.
static RTC_NOINIT_ATTR uint32_t setupRequest;
static constexpr uint32_t SETUP_REQUESTED = 0x5e7a9b0f;

void Provisioning::requestAtNextBoot() { setupRequest = SETUP_REQUESTED; }

bool Provisioning::takeRequest() {
  const bool requested = setupRequest == SETUP_REQUESTED;
  setupRequest = 0;
  return requested;
}

void Provisioning::begin(SettingsStore& store, const UnitSettings& settings) {
  store_ = &store;
  draft_ = settings;
  active_ = true;

  // The end of the MAC address, so two controllers being set up in the same
  // room are told apart.
  const uint32_t id = static_cast<uint32_t>(ESP.getEfuseMac() >> 24) & 0xFFFFFF;
  char ssid[33];
  snprintf(ssid, sizeof(ssid), "%s%06lx", SETUP_AP_PREFIX, static_cast<unsigned long>(id));

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid);  // no password: see provisioning.h

  // Answer every lookup with our own address, so a phone's captive-portal
  // check opens the page by itself.
  dns_.start(53, "*", WiFi.softAPIP());

  server_.on("/", [this]() { handleRoot(); });
  server_.on("/save", HTTP_POST, [this]() { handleSave(); });
  server_.onNotFound([this]() { handleRoot(); });
  server_.begin();

  Serial.printf("Setup: join \"%s\" (no password), then open http://%s/\n", ssid,
                WiFi.softAPIP().toString().c_str());
}

void Provisioning::tick() {
  if (!active_) return;
  dns_.processNextRequest();
  server_.handleClient();
}

void Provisioning::handleRoot() {
  String page = settings_form::pageStart("Mini-split setup");
  page += settings_form::form(draft_);
  page += settings_form::PAGE_END;
  server_.send(200, "text/html", page);
}

void Provisioning::handleSave() {
  settings_form::read(server_, draft_);
  store_->save(draft_);

  server_.send(200, "text/html",
               settings_form::saved("Restarting and joining your network. This page is about to "
                                    "disappear with it."));

  Serial.printf("Setup: saved, joining \"%s\" as \"%s\"\n", draft_.wifiSsid, draft_.name);
  delay(500);  // let the page reach the phone before the radio goes
  ESP.restart();
}
