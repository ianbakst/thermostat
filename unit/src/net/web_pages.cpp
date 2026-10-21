#include "net/web_pages.h"

#include <Arduino.h>
#include <esp_timer.h>

#include "config.h"
#include "net/control_page.h"
#include "net/own_address.h"
#include "net/settings_form.h"

// The secondary button on the settings page (Log out), next to the form's
// blue one.
static const char* SETTINGS_STYLE = "button.quiet{background:#333;margin-top:12px}";

void WebPages::begin(SettingsStore& store, const UnitSettings& settings,
                     const HeatPumpLink& heatPump, const WifiConnection& wifi,
                     const MqttConnection& mqtt, PageLogin& login) {
  if (started_) return;
  started_ = true;
  store_ = &store;
  settings_ = &settings;
  heatPump_ = &heatPump;
  wifi_ = &wifi;
  mqtt_ = &mqtt;
  login_ = &login;

  // The server only keeps the headers it's told to.
  static const char* headers[] = {"Origin", "Cookie"};
  server_.collectHeaders(headers, 2);

  server_.on("/", HTTP_GET, [this]() { handleControls(); });
  server_.on("/settings", HTTP_GET, [this]() { handleSettings(); });
  server_.on("/save", HTTP_POST, [this]() { handleSave(); });
  server_.on("/login", HTTP_GET, [this]() { handleLoginPage(); });
  server_.on("/login", HTTP_POST, [this]() { handleLogin(); });
  server_.on("/logout", HTTP_POST, [this]() { handleLogout(); });
  server_.begin();

  Serial.printf("Web pages: http://%s.local/\n", settings.name);
  if (!login.hasPassword()) {
    Serial.println("Web pages: no settings password yet, so the settings are shut until one is "
                   "chosen in setup mode");
  }
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

// Anything that changes something must come from this controller's own page,
// or from no page at all (net/own_address.h).
bool WebPages::fromOwnPage() {
  return !server_.hasHeader("Origin") || isOwnOrigin(server_.header("Origin"), settings_->name);
}

// The browser's login, if it has a good one; empty otherwise.
String WebPages::currentLogin() {
  const String login = PageLogin::loginIn(server_.header("Cookie"));
  return login_->isLogin(login) ? login : "";
}

void WebPages::redirect(const char* path) {
  server_.sendHeader("Location", path);
  server_.send(303, "text/plain", "");
}

void WebPages::handleControls() {
  String page = settings_form::pageStart(settings_->name, CONTROL_PAGE_STYLE);
  page += addressNote();
  page += CONTROL_PAGE_BODY;
  page += settings_form::PAGE_END;
  server_.send(200, "text/html", page);
}

// ---------------------------------------------------------------------------
// Logging in
// ---------------------------------------------------------------------------

void WebPages::showNoPassword() {
  String page = settings_form::pageStart(settings_->name);
  page += "<p>This unit's settings have no password yet, so they stay shut. Hold its BOOT "
          "button for ";
  page += SETUP_BUTTON_HOLD_MS / 1'000;
  page += " seconds: it restarts into setup mode, where you can choose one.</p>"
          "<p class=sub>The <a href=/>controls</a> work without it.</p>";
  page += settings_form::PAGE_END;
  server_.send(200, "text/html", page);
}

void WebPages::showLogin(const String& message) {
  String page = settings_form::pageStart(settings_->name);
  page += addressNote();
  if (!message.isEmpty()) {
    page += "<p class=error>";
    page += settings_form::escapeHtml(message.c_str());
    page += "</p>";
  }
  page += "<form method=POST action=/login>"
          "<label for=pw>Settings password</label>"
          "<input id=pw name=pw type=password autocomplete=current-password autofocus>"
          "<button type=submit>Log in</button></form>"
          "<p class=hint>Forgotten it? Hold the unit's BOOT button for ";
  page += SETUP_BUTTON_HOLD_MS / 1'000;
  page += " seconds and choose a new one in setup mode.</p>"
          "<p class=sub><a href=/>Controls</a>, which need no password</p>";
  page += settings_form::PAGE_END;
  server_.send(200, "text/html", page);
}

void WebPages::handleLoginPage() {
  if (!login_->hasPassword()) {
    showNoPassword();
  } else if (!currentLogin().isEmpty()) {
    redirect("/settings");
  } else {
    showLogin("");
  }
}

void WebPages::handleLogin() {
  if (!fromOwnPage()) {
    server_.send(403, "text/plain", "Logging in only works from this unit's own page.\n");
    return;
  }
  if (!login_->hasPassword()) {
    showNoPassword();
    return;
  }

  const String from = server_.client().remoteIP().toString();
  switch (login_->check(server_.arg("pw").c_str(), millis())) {
    case PageLogin::Check::Right:
      Serial.printf("Login: %s logged in\n", from.c_str());
      server_.sendHeader("Set-Cookie", PageLogin::cookieFor(login_->newLogin()));
      redirect("/settings");
      break;
    case PageLogin::Check::Wrong:
      Serial.printf("Login: wrong password from %s\n", from.c_str());
      showLogin("That isn't the password.");
      break;
    case PageLogin::Check::LockedOut:
      showLogin(String("Too many wrong passwords. Wait ") + PAGE_LOGIN_LOCKOUT_MS / 1'000 +
                " seconds and try again.");
      break;
  }
}

// Takes the login back from this browser. A copy of it elsewhere would still
// work until the password changes: see net/page_login.h.
void WebPages::handleLogout() {
  if (!fromOwnPage()) {
    server_.send(403, "text/plain", "Logging out only works from this unit's own page.\n");
    return;
  }
  server_.sendHeader("Set-Cookie", PageLogin::cookieFor(""));
  redirect("/");
}

// ---------------------------------------------------------------------------
// The settings
// ---------------------------------------------------------------------------

// One line of the status list at the top of the settings page.
static void row(String& page, const char* label, const char* value) {
  page += "<dt>";
  page += label;
  page += "</dt><dd>";
  page += settings_form::escapeHtml(value);
  page += "</dd>";
}

void WebPages::handleSettings() {
  if (!login_->hasPassword()) {
    showNoPassword();
    return;
  }
  const String login = currentLogin();
  if (login.isEmpty()) {
    redirect("/login");
    return;
  }

  // Handing the same login back with a fresh expiry, so a browser that's
  // used keeps it.
  server_.sendHeader("Set-Cookie", PageLogin::cookieFor(login));
  showSettings(*settings_, "");
}

// The settings page, with the form filled in from `shown`: the saved
// settings, or what was just posted if `error` says why it wasn't saved.
void WebPages::showSettings(const UnitSettings& shown, const String& error) {
  const UnitSettings& settings = *settings_;
  char text[192];

  String page = settings_form::pageStart(settings.name, SETTINGS_STYLE);
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
  page += settings_form::form(shown, *login_, settings_form::PasswordBoxes::Change, error);
  page += "<form method=POST action=/logout><button type=submit class=quiet>Log out</button>"
          "</form>";
  page += settings_form::PAGE_END;
  server_.send(200, "text/html", page);
}

void WebPages::handleSave() {
  if (!fromOwnPage()) {
    server_.send(403, "text/plain", "Settings can only be saved from this unit's own page.\n");
    return;
  }
  if (!login_->hasPassword()) {
    showNoPassword();
    return;
  }
  if (currentLogin().isEmpty()) {
    redirect("/login");
    return;
  }

  UnitSettings draft = *settings_;
  settings_form::read(server_, draft);

  String password;
  const String error = settings_form::readPagePassword(
      server_, *login_, settings_form::PasswordBoxes::Change, millis(), password);
  if (!error.isEmpty()) {
    showSettings(draft, error);
    return;
  }

  store_->save(draft);
  if (!password.isEmpty()) {
    // That logs out every browser, this one included, so this one gets a
    // login under the new password.
    login_->setPassword(password.c_str());
    server_.sendHeader("Set-Cookie", PageLogin::cookieFor(login_->newLogin()));
  }

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
