#include "net/settings_form.h"

#include <cstring>

#include "config.h"

namespace settings_form {

String escapeHtml(const char* text) {
  String out;
  for (const char* c = text; *c; c++) {
    switch (*c) {
      case '&':  out += "&amp;"; break;
      case '<':  out += "&lt;"; break;
      case '>':  out += "&gt;"; break;
      case '"':  out += "&quot;"; break;
      default:   out += *c;
    }
  }
  return out;
}

String pageStart(const char* title, const char* style) {
  String out;
  out.reserve(4096);  // enough for the settings page, so it isn't copied as it grows
  out += "<!DOCTYPE html><html><head><meta charset=utf-8>"
         "<meta name=viewport content=\"width=device-width,initial-scale=1\"><title>";
  out += escapeHtml(title);
  out += "</title><style>"
         "body{font-family:system-ui,sans-serif;margin:0 auto;padding:20px;max-width:26em;"
         "background:#111;color:#eee}h1{font-size:20px;margin:4px 0 0}"
         "h2{font-size:16px;margin:28px 0 0}a{color:#64b5f6}"
         "label,dt{display:block;margin:14px 0 4px;font-size:14px;color:#aaa}dd{margin:0}"
         "input{width:100%;box-sizing:border-box;padding:10px;font-size:16px;border-radius:6px;"
         "border:1px solid #444;background:#222;color:#eee}"
         "button{margin-top:22px;width:100%;padding:14px;font-size:16px;border:0;border-radius:6px;"
         "background:#1976d2;color:#fff}"
         ".hint,.sub{color:#999;font-size:13px;line-height:1.4;margin:4px 0 0}"
         ".error{color:#ffab91;margin:16px 0 0}";
  out += style;
  out += "</style></head><body><h1>";
  out += escapeHtml(title);
  out += "</h1>";
  return out;
}

static String field(const char* label, const char* name, const char* value, const char* hint,
                    const char* type = "text") {
  String out = "<label for=";
  out += name;
  out += ">";
  out += label;
  out += "</label><input id=";
  out += name;
  out += " name=";
  out += name;
  out += " type=";
  out += type;
  out += " value=\"";
  out += escapeHtml(value);
  out += "\">";
  if (hint) {
    out += "<p class=hint>";
    out += hint;
    out += "</p>";
  }
  return out;
}

String form(const UnitSettings& settings, const PageLogin& login, PasswordBoxes boxes,
            const String& error) {
  String out;
  if (!error.isEmpty()) {
    out += "<p class=error>";
    out += escapeHtml(error.c_str());
    out += " Nothing was saved, and any passwords need typing again.</p>";
  }
  out += "<form method=POST action=/save>";
  out += field("Wi-Fi network", "ssid", settings.wifiSsid, nullptr);
  out += field("Wi-Fi password", "wpw", "",
               settings.wifiPassword[0] ? "Leave empty to keep the saved one. A different "
                                          "network needs its own."
                                        : nullptr,
               "password");
  out += field("Name for this unit", "name", settings.name,
               "What Home Assistant and the wall panels call it, e.g. OfficeHVAC. Replacing a "
               "board means reusing the name it had.");
  out += field("MQTT broker", "host", settings.mqttHost,
               "Home Assistant's Mosquitto, normally homeassistant.local. Not port 8123, which "
               "is HA's web UI.");
  out += field("Broker port", "port", String(settings.mqttPort).c_str(), nullptr);
  out += field("Broker username", "muser", settings.mqttUser, nullptr);
  out += field("Broker password", "mpw", "",
               settings.mqttPassword[0] ? "Leave empty to keep the saved one. A different "
                                          "broker or username needs its own."
                                        : nullptr,
               "password");
  out += field("Temperatures in", "tu", settings.useFahrenheit ? "F" : "C", "F or C.");
  out += field("Lowest setpoint", "min", String(settings.minTemp, 0).c_str(), nullptr);
  out += field("Highest setpoint", "max", String(settings.maxTemp, 0).c_str(), nullptr);
  out += field("Setpoint step", "step", String(settings.tempStep, 1).c_str(), nullptr);
  out += field("Home Assistant id", "haid", settings.haUniqueId,
               "Leave blank for a new unit. When replacing a board, put the old one's unique_id "
               "here and Home Assistant keeps the entity it already has.");

  const String minimum = String("At least ") + PAGE_PASSWORD_MIN_LENGTH + " characters.";
  if (boxes == PasswordBoxes::Setup) {
    const String hint =
        login.hasPassword()
            ? "Leave empty to keep the saved one. Typing one replaces it, even if the old one "
              "is forgotten."
            : "Guards this unit's settings and firmware updates. The controls stay open to "
              "anyone on your network. " + minimum;
    out += field("Settings password", "ppw", "", hint.c_str(), "password");
    out += field("Settings password again", "ppw2", "", nullptr, "password");
  } else {
    out += field("New settings password", "ppw", "",
                 "Leave empty to keep the current one. Changing it logs out every other "
                 "browser.",
                 "password");
    out += field("New settings password again", "ppw2", "", nullptr, "password");
    out += field("Current settings password", "cpw", "", "Only needed to change it.", "password");
  }

  out += "<button type=submit>Save and restart</button></form>";
  return out;
}

// Copies one form field into a fixed buffer, leaving what's there if the
// field wasn't sent at all.
static void readArg(WebServer& server, const char* name, char* out, size_t size) {
  if (!server.hasArg(name)) return;
  snprintf(out, size, "%s", server.arg(name).c_str());
}

// A password box left empty keeps the saved password, if it's still for the
// same network or login. A box that wasn't sent at all counts as empty, so a
// hand-made request can't use its absence to keep one either.
static void readPassword(WebServer& server, const char* name, char* out, size_t size,
                         bool sameLogin) {
  const String typed = server.arg(name);  // empty if it wasn't sent
  if (typed.isEmpty() && sameLogin) return;
  snprintf(out, size, "%s", typed.c_str());
}

void read(WebServer& server, UnitSettings& settings) {
  const UnitSettings before = settings;

  readArg(server, "ssid", settings.wifiSsid, sizeof(settings.wifiSsid));
  readArg(server, "name", settings.name, sizeof(settings.name));
  readArg(server, "host", settings.mqttHost, sizeof(settings.mqttHost));
  readArg(server, "muser", settings.mqttUser, sizeof(settings.mqttUser));
  readArg(server, "haid", settings.haUniqueId, sizeof(settings.haUniqueId));
  if (server.hasArg("port")) settings.mqttPort = server.arg("port").toInt();
  if (server.hasArg("tu")) {
    const String unit = server.arg("tu");
    settings.useFahrenheit = unit.startsWith("F") || unit.startsWith("f");
  }
  if (server.hasArg("min")) settings.minTemp = server.arg("min").toFloat();
  if (server.hasArg("max")) settings.maxTemp = server.arg("max").toFloat();
  if (server.hasArg("step")) settings.tempStep = server.arg("step").toFloat();

  readPassword(server, "wpw", settings.wifiPassword, sizeof(settings.wifiPassword),
               strcmp(before.wifiSsid, settings.wifiSsid) == 0);
  readPassword(server, "mpw", settings.mqttPassword, sizeof(settings.mqttPassword),
               strcmp(before.mqttHost, settings.mqttHost) == 0 &&
                   strcmp(before.mqttUser, settings.mqttUser) == 0);
}

String readPagePassword(WebServer& server, PageLogin& login, PasswordBoxes boxes,
                        unsigned long nowMs, String& password) {
  password = server.arg("ppw");
  if (password.isEmpty()) {
    return login.hasPassword() ? "" : "Choose a settings password: the settings can't be opened "
                                      "without one.";
  }
  if (password.length() < PAGE_PASSWORD_MIN_LENGTH) {
    return String("The settings password needs at least ") + PAGE_PASSWORD_MIN_LENGTH +
           " characters.";
  }
  if (password != server.arg("ppw2")) return "The two new settings passwords don't match.";

  if (boxes == PasswordBoxes::Change) {
    switch (login.check(server.arg("cpw").c_str(), nowMs)) {
      case PageLogin::Check::Right: break;
      case PageLogin::Check::Wrong: return "The current settings password isn't right.";
      case PageLogin::Check::LockedOut:
        return String("Too many wrong passwords. Wait ") + PAGE_LOGIN_LOCKOUT_MS / 1'000 +
               " seconds and try again.";
    }
  }
  return "";
}

String saved(const String& message) {
  String out = pageStart("Saved");
  out += "<p>";
  out += message;
  out += "</p>";
  out += PAGE_END;
  return out;
}

}  // namespace settings_form
