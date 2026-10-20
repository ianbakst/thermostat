#include "net/web_pages.h"

#include <Arduino.h>
#include <esp_timer.h>

#include "config.h"
#include "net/control_page.h"
#include "net/own_address.h"
#include "net/settings_form.h"

void WebPages::begin(SettingsStore& store, const UnitSettings& settings,
                     const HeatPumpLink& heatPump, const WifiConnection& wifi,
                     const MqttConnection& mqtt) {
  if (started_) return;
  started_ = true;
  store_ = &store;
  settings_ = &settings;
  heatPump_ = &heatPump;
  wifi_ = &wifi;
  mqtt_ = &mqtt;

  // The server only keeps the headers it's told to.
  static const char* headers[] = {"Origin"};
  server_.collectHeaders(headers, 1);

  server_.on("/", HTTP_GET, [this]() { handleControls(); });
  server_.on("/settings", HTTP_GET, [this]() { handleSettings(); });
  server_.on("/save", HTTP_POST, [this]() { handleSave(); });
  server_.begin();

  Serial.printf("Web pages: http://%s.local/\n", settings.name);
}

void WebPages::tick() {
  if (started_) server_.handleClient();
}

// A page opened at some other address — a router's own name for the
// controller, say — still loads, but its controls and saving are refused
// there (net/own_address.h). So it says where they do work.
String WebPages::addressNote() {
  if (isOwnHost(server_.hostHeader(), settings_->name)) return "";
  const String name = settings_form::escapeHtml(settings_->name);
  return "<p class=hint>Open this page at <a href=\"http://" + name + ".local/\">" + name +
         ".local</a> to use it. Other addresses are turned away, to keep other websites out.</p>";
}

void WebPages::handleControls() {
  String page = settings_form::pageStart(settings_->name, CONTROL_PAGE_STYLE);
  page += addressNote();
  page += CONTROL_PAGE_BODY;
  page += settings_form::PAGE_END;
  server_.send(200, "text/html", page);
}

// One line of the status list at the top of the settings page.
static void row(String& page, const char* label, const char* value) {
  page += "<dt>";
  page += label;
  page += "</dt><dd>";
  page += settings_form::escapeHtml(value);
  page += "</dd>";
}

void WebPages::handleSettings() {
  const UnitSettings& settings = *settings_;
  char text[192];

  String page = settings_form::pageStart(settings.name);
  page += addressNote();

  // esp_timer rather than millis(), which wraps after 49 days.
  const unsigned long minutes = esp_timer_get_time() / 60'000'000;
  snprintf(text, sizeof(text), "Mini-split controller " FIRMWARE_VERSION ", up %lud %luh %lum",
           minutes / 1440, minutes / 60 % 24, minutes % 60);
  page += "<p class=sub>";
  page += text;
  page += " &middot; <a href=/>Controls</a></p><dl>";

  heatPump_->state().describe(text, sizeof(text));
  row(page, "Heat pump", text);

  const WifiStatus wifi = wifi_->status();
  snprintf(text, sizeof(text), "%s, %s, signal %d dBm", wifi.ssid, wifi.ip, wifi.rssi);
  row(page, "Wi-Fi", text);

  snprintf(text, sizeof(text), "%s, %s:%u",
           mqtt_->connected()      ? "connected"
           : mqtt_->loginRefused() ? "the broker refused the login"
                                   : "not connected",
           settings.mqttHost, settings.mqttPort);
  row(page, "MQTT", text);

  page += "</dl><h2>Settings</h2>";
  page += settings_form::form(settings);
  page += settings_form::PAGE_END;
  server_.send(200, "text/html", page);
}

void WebPages::handleSave() {
  if (server_.hasHeader("Origin") && !isOwnOrigin(server_.header("Origin"), settings_->name)) {
    server_.send(403, "text/plain", "Settings can only be saved from this unit's own page.\n");
    return;
  }

  UnitSettings draft = *settings_;
  settings_form::read(server_, draft);
  store_->save(draft);

  // The name may just have changed, and the address with it.
  const String name = settings_form::escapeHtml(draft.name);
  String message = "Restarting with the new settings. It'll be back in a few seconds at ";
  message += "<a href=\"http://" + name + ".local/\">" + name + ".local</a>. ";
  message += "If it doesn't come back, it couldn't join the network: hold its BOOT button for ";
  message += SETUP_BUTTON_HOLD_MS / 1'000;
  message += " seconds for setup mode.";
  server_.send(200, "text/html", settings_form::saved(message));

  Serial.println("Web pages: settings saved, restarting");
  delay(500);  // let the page reach the browser before the restart
  ESP.restart();
}
