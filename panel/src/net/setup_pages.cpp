#include "net/setup_pages.h"

#include "config.h"

namespace setup_pages {
namespace {

// Values go into HTML attributes and text, and a network name can contain
// quotes or angle brackets.
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

// Every page opens the same way. `reloadItself` is for the pages that wait on
// a test: reloading also recovers the view if the phone briefly lost the
// panel's network.
String top(const char* stepLabel, bool reloadItself) {
  String out;
  out.reserve(1400);  // the style block alone is ~700 bytes
  out += "<!DOCTYPE html><html><head><meta charset=utf-8>"
         "<meta name=viewport content=\"width=device-width,initial-scale=1\">";
  if (reloadItself) out += "<meta http-equiv=refresh content=2>";
  out += "<title>Thermostat setup</title><style>"
         "body{font-family:system-ui,sans-serif;margin:0 auto;padding:20px;max-width:26em;"
         "background:#111;color:#eee}h1{font-size:20px;margin:4px 0 0}"
         "label{display:block;margin:14px 0 4px;font-size:14px;color:#aaa}"
         "input{width:100%;box-sizing:border-box;padding:10px;font-size:16px;border-radius:6px;"
         "border:1px solid #444;background:#222;color:#eee}"
         "button{margin-top:22px;width:100%;padding:14px;font-size:16px;border:0;border-radius:6px;"
         "background:#1976d2;color:#fff}"
         ".units button{margin-top:8px;background:#222;border:1px solid #444;text-align:left}"
         ".step{color:#888;font-size:12px;letter-spacing:.1em;text-transform:uppercase;margin:0}"
         ".hint{color:#999;font-size:13px;line-height:1.4;margin:4px 0 0}"
         ".error{background:#5c0e0e;padding:10px;border-radius:6px;font-size:14px;margin-top:14px}"
         "a{color:#64b5f6}</style></head><body><p class=step>";
  out += stepLabel;
  out += "</p>";
  return out;
}

String errorBanner(const char* error) {
  if (!error) return "";
  return "<p class=error>" + escapeHtml(error) + "</p>";
}

String backLink(bool canGoBack) {
  return canGoBack ? "<p><a href=/back>Back</a></p>" : "";
}

// A blank password field means "keep the saved one", so a password can't be
// wiped by resubmitting a form (the browser never receives it). Only offer
// that when there is one to keep.
const char* passwordPlaceholder(const char* saved) {
  return saved[0] ? "leave blank to keep the saved one" : "required";
}

String close() { return "</body></html>"; }

}  // namespace

String wifiForm(const Context& context) {
  const DeviceSettings& draft = context.draft;
  return top(context.stepLabel, false) + "<h1>Wi-Fi</h1>" + errorBanner(context.error) +
         "<form method=POST action=/wifi>"
         "<label>Network (2.4GHz)</label>"
         "<input name=ssid list=nets required value=\"" + escapeHtml(draft.wifiSsid) + "\">"
         "<datalist id=nets>" + (context.networks ? *context.networks : String()) + "</datalist>"
         "<label>Password</label>"
         "<input name=wifipass type=password placeholder=\"" +
         passwordPlaceholder(draft.wifiPassword) +
         "\"><label>Name for this panel (optional)</label>"
         "<input name=hostname placeholder=\"" + String(DEFAULT_HOSTNAME) + "\" value=\"" +
         escapeHtml(draft.hostname) +
         "\"><p class=hint>What the panel calls itself on your network: the name in your router's "
         "device list, and &lt;name&gt;.local. Not the mini-split's name.</p>"
         "<button type=submit>Next</button></form>" +
         close();
}

String wifiTesting(const Context& context) {
  return top(context.stepLabel, true) + "<h1>Connecting...</h1>"
         "<p class=hint>Joining \"" + escapeHtml(context.draft.wifiSsid) +
         "\". This page reloads by itself.</p>"
         "<p class=hint>If it stops loading, rejoin \"" + escapeHtml(context.apSsid) +
         "\" — the panel changes Wi-Fi channel as it connects, which can drop your phone for a "
         "moment.</p>" + backLink(context.canGoBack) + close();
}

String mqttForm(const Context& context) {
  const DeviceSettings& draft = context.draft;
  return top(context.stepLabel, false) + "<h1>MQTT</h1>" + errorBanner(context.error) +
         "<p class=hint>Wi-Fi is connected. Now the broker Home Assistant uses.</p>"
         "<form method=POST action=/mqtt>"
         "<label>Broker</label>"
         "<input name=mqtthost required value=\"" + escapeHtml(draft.mqttHost) + "\">"
         "<label>Port</label>"
         "<input name=mqttport type=number required value=\"" + String(draft.mqttPort) +
         "\"><p class=hint>Usually homeassistant.local on port 1883. Not port 8123, which is HA's "
         "web interface.</p>"
         "<label>Username</label>"
         "<input name=mqttuser value=\"" + escapeHtml(draft.mqttUser) + "\">"
         "<label>Password</label>"
         "<input name=mqttpass type=password placeholder=\"" +
         passwordPlaceholder(draft.mqttPassword) +
         "\"><p class=hint>The broker login your mini-splits use. Leave both blank only if your "
         "broker allows anonymous clients.</p>"
         "<button type=submit>Next</button></form>" +
         backLink(context.canGoBack) + close();
}

String mqttTesting(const Context& context) {
  return top(context.stepLabel, true) + "<h1>Connecting...</h1>"
         "<p class=hint>Reaching " + escapeHtml(context.draft.mqttHost) +
         ". This page reloads by itself.</p>" + backLink(context.canGoBack) + close();
}

String unitChoice(const Context& context, const char* const* units, int count) {
  // The list is whatever Home Assistant has advertised on the discovery topic
  // since the broker connected, which takes a moment to arrive. While it's
  // empty the page reloads itself, waiting for them.
  String out = top(context.stepLabel, count == 0) + "<h1>Mini-split</h1>";
  if (count == 0) {
    out += "<p class=hint>Waiting for Home Assistant to say which mini-splits exist. This page "
           "reloads by itself.</p>";
  } else {
    out += "<p class=hint>Which unit does this panel control?</p>"
           "<form class=units method=POST action=/unit>";
    for (int i = 0; i < count; i++) {
      const String name = escapeHtml(units[i]);
      out += "<button type=submit name=unit value=\"" + name + "\">" + name + "</button>";
    }
    out += "</form>";
  }
  out += "<form method=POST action=/unit><button type=submit name=unit value=\"\">"
         "Skip, I'll pick on the panel</button></form>";
  return out + backLink(context.canGoBack) + close();
}

String done(const Context& context, bool apWillDisappear) {
  return top(context.stepLabel, false) + "<h1>All set</h1><p class=hint>The panel is restarting" +
         (apWillDisappear ? ". This setup network is about to disappear." : ".") + "</p>" + close();
}

}  // namespace setup_pages
