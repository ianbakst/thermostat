#include "net/web_pages.h"

#include <Arduino.h>
#include <esp_timer.h>

#include "config.h"
#include "net/control_page.h"
#include "net/own_address.h"
#include "net/settings_form.h"

// The secondary buttons on the settings page (Log out, the firmware ones),
// next to the form's blue one.
static const char* SETTINGS_STYLE = "button.quiet{background:#333;margin-top:12px}";

void WebPages::begin(SettingsStore& store, const UnitSettings& settings,
                     const HeatPumpLink& heatPump, const WifiConnection& wifi,
                     const MqttConnection& mqtt, PageLogin& login, OtaUpdater& ota) {
  if (started_) return;
  started_ = true;
  store_ = &store;
  settings_ = &settings;
  heatPump_ = &heatPump;
  wifi_ = &wifi;
  mqtt_ = &mqtt;
  login_ = &login;
  ota_ = &ota;

  // The server only keeps the headers it's told to.
  static const char* headers[] = {"Origin", "Cookie"};
  server_.collectHeaders(headers, 2);

  server_.on("/", HTTP_GET, [this]() { handleControls(); });
  server_.on("/settings", HTTP_GET, [this]() { handleSettings(); });
  server_.on("/save", HTTP_POST, [this]() { handleSave(); });
  server_.on("/login", HTTP_GET, [this]() { handleLoginPage(); });
  server_.on("/login", HTTP_POST, [this]() { handleLogin(); });
  server_.on("/logout", HTTP_POST, [this]() { handleLogout(); });
  server_.on("/update/check", HTTP_POST, [this]() { handleUpdateCheck(); });
  server_.on("/update/install", HTTP_POST, [this]() { handleUpdateInstall(); });
  server_.on("/update/channel", HTTP_POST, [this]() { handleUpdateChannel(); });
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

// For anything that changes something: it has to come from this unit's own
// page, from a browser that's logged in. When it doesn't, this sends the
// answer itself and returns false.
bool WebPages::mayChange() {
  if (!fromOwnPage()) {
    server_.send(403, "text/plain", "That only works from this unit's own page.\n");
    return false;
  }
  if (!login_->hasPassword()) {
    showNoPassword();
    return false;
  }
  if (currentLogin().isEmpty()) {
    redirect("/login");
    return false;
  }
  return true;
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

  // While a check or a download runs, the page keeps reloading to follow it.
  // The address is spelled out, without a #fragment: told to go to the page
  // it's already on plus a fragment, a browser only scrolls, and the page
  // would sit at "Checking" for good.
  const OtaState state = ota_->status().state;
  if (state == OtaState::Checking || state == OtaState::Downloading) {
    server_.sendHeader("Refresh", String(OTA_PAGE_REFRESH_S) + "; url=/settings");
  }
  showSettings(*settings_, "");
}

// What the updater is doing, in a line.
static String updateLine(const OtaStatus& status, OtaChannel channel) {
  const String version = settings_form::escapeHtml(status.version);
  switch (status.state) {
    case OtaState::Idle:
      return "Not checked yet. It checks by itself a minute after joining the network, then "
             "once a day.";
    case OtaState::Checking: return "Checking&hellip;";
    case OtaState::UpToDate:
      return "Up to date: " + version + " is the newest on the " + channelName(channel) +
             " channel.";
    case OtaState::Available: return version + " is available.";
    case OtaState::Downloading: {
      String line = "Installing " + version;
      if (status.progress >= 0) line += String(": ") + String(status.progress) + "%";
      return line + ". The unit restarts into it when it's done: reload this page then.";
    }
    case OtaState::Installed: return version + " is installed. Restarting into it&hellip;";
    case OtaState::Failed:
      return "The last try didn't work: " + settings_form::escapeHtml(status.message) + ".";
  }
  return "";
}

// One of the firmware section's buttons: a form of its own, posting to `path`.
static String button(const char* path, const String& label, const char* name = nullptr,
                     const char* value = nullptr) {
  String out = "<form method=POST action=";
  out += path;
  out += ">";
  if (name) {
    out += "<input type=hidden name=";
    out += name;
    out += " value=";
    out += value;
    out += ">";
  }
  out += "<button type=submit class=quiet>";
  out += label;
  out += "</button></form>";
  return out;
}

String WebPages::firmwareSection() {
  const OtaStatus status = ota_->status();
  const OtaChannel channel = ota_->channel();
  const bool busy = status.state == OtaState::Checking || status.state == OtaState::Downloading ||
                    status.state == OtaState::Installed;

  String out = "<h2>Firmware</h2><dl>";
  row(out, "Running", FIRMWARE_VERSION);
  row(out, "Channel", channel == OtaChannel::Beta
                          ? "beta: a build of every change to dev, for trying things out"
                          : "stable: tagged releases");
  out += "<dt>Updates</dt><dd>";
  out += updateLine(status, channel);
  out += "</dd></dl>";

  if (!busy) {
    if (status.state == OtaState::Available) {
      out += button("/update/install", "Install " + settings_form::escapeHtml(status.version));
    }
    out += button("/update/check", "Check now");
    out += channel == OtaChannel::Beta
               ? button("/update/channel", "Switch to stable", "channel", "stable")
               : button("/update/channel", "Switch to beta", "channel", "beta");
  }
  out += "<p class=hint>Updates come only from this project's releases on GitHub, and nothing "
         "installs until you press Install. For its first minute, new firmware is on trial: if "
         "it can't join Wi-Fi, or the unit restarts before then, it goes back to the one "
         "before.</p>";
  return out;
}

void WebPages::handleUpdateCheck() {
  if (!mayChange()) return;
  ota_->check(millis());
  redirect("/settings");
}

void WebPages::handleUpdateInstall() {
  if (!mayChange()) return;
  // Only ever what the last check found, from the channel's manifest:
  // there's nothing here to say where the firmware comes from.
  Serial.printf("Update: installing %s, asked for by %s\n", ota_->status().version,
                server_.client().remoteIP().toString().c_str());
  ota_->install();
  redirect("/settings");
}

void WebPages::handleUpdateChannel() {
  if (!mayChange()) return;
  const OtaChannel channel =
      server_.arg("channel") == "beta" ? OtaChannel::Beta : OtaChannel::Stable;
  if (channel != ota_->channel()) {
    store_->saveOtaChannel(channel);
    ota_->setChannel(channel);
    Serial.printf("Update: now on the %s channel\n", channelName(channel));
  }
  ota_->check(millis());
  redirect("/settings");
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

  page += "</dl>";
  page += firmwareSection();
  page += "<h2>Settings</h2>";
  page += settings_form::form(shown, *login_, settings_form::PasswordBoxes::Change, error);
  page += "<form method=POST action=/logout><button type=submit class=quiet>Log out</button>"
          "</form>";
  page += settings_form::PAGE_END;
  server_.send(200, "text/html", page);
}

void WebPages::handleSave() {
  if (!mayChange()) return;

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
