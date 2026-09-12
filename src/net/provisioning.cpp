#include "net/provisioning.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_random.h>

#include "config.h"

// Letters and digits that can't be confused with each other when read off the
// panel and typed into a phone (no l/1, O/0).
static const char PASSWORD_CHARS[] = "abcdefghijkmnpqrstuvwxyz23456789";

static void randomPassword(char* out, size_t length) {
  for (size_t i = 0; i < length; i++) {
    out[i] = PASSWORD_CHARS[esp_random() % (sizeof(PASSWORD_CHARS) - 1)];
  }
  out[length] = '\0';
}

// Values go into HTML attributes and text, and a network name can contain
// quotes or angle brackets.
static String escapeHtml(const char* text) {
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

// One plain page per step, no external files: a phone in setup mode has no
// internet access, so anything it couldn't fetch from us wouldn't load at all.
static String pageTop(const char* step, bool reloadItself) {
  String top = "<!DOCTYPE html><html><head><meta charset=utf-8>"
               "<meta name=viewport content=\"width=device-width,initial-scale=1\">";
  // The pages that wait on a test reload themselves, which also recovers the
  // view if the phone briefly lost the panel's network.
  if (reloadItself) top += "<meta http-equiv=refresh content=2>";
  top += "<title>Thermostat setup</title><style>"
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
         "a{color:#64b5f6}</style></head><body>";
  top += "<p class=step>" + String(step) + "</p>";
  return top;
}

void Provisioning::begin(Mode mode, const DeviceSettings& current, OnFinished onFinished,
                         void* context) {
  if (active_) return;
  mode_ = mode;
  draft_ = current;
  onFinished_ = onFinished;
  context_ = context;
  step_ = mode == Mode::Full ? Step::Wifi : Step::Mqtt;
  error_ = nullptr;
  networkOptions_ = "";

  if (mode == Mode::Full) {
    // Scan before the access point starts. Scanning takes the radio away for a
    // couple of seconds, which would interrupt a phone that had already joined.
    const int found = WiFi.scanNetworks();
    for (int i = 0; i < found; i++) {
      const String ssid = WiFi.SSID(i);
      if (ssid.isEmpty()) continue;  // hidden network: nothing to show
      const String option = "<option value=\"" + escapeHtml(ssid.c_str()) + "\">";
      if (networkOptions_.indexOf(option) < 0) networkOptions_ += option;  // same network on 2.4 and 5GHz
    }
    WiFi.scanDelete();
    Serial.printf("Setup: %d networks in range\n", found);

    snprintf(apSsid_, sizeof(apSsid_), SETUP_AP_PREFIX "%06llX",
             static_cast<unsigned long long>(ESP.getEfuseMac() >> 24) & 0xFFFFFF);
    randomPassword(apPassword_, SETUP_AP_PASSWORD_LEN);
    snprintf(joinQr_, sizeof(joinQr_), "WIFI:T:WPA;S:%s;P:%s;;", apSsid_, apPassword_);

    // AP_STA, not AP: the panel needs a station connection of its own to test
    // the network in step 1 while the phone stays on the access point.
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apSsid_, apPassword_);

    dns_.setTTL(0);  // don't let a phone cache these answers after setup
    dns_.start(53, "*", WiFi.softAPIP());

    snprintf(pageUrl_, sizeof(pageUrl_), "http://%s/", WiFi.softAPIP().toString().c_str());
  } else {
    // Only the broker is being changed, and the network already works, so the
    // pages go out on it: the phone stays where it is, and there's no access
    // point or DNS server to run.
    snprintf(pageUrl_, sizeof(pageUrl_), "http://%s/", WiFi.localIP().toString().c_str());
  }

  server_.on("/", HTTP_GET, [this]() { sendCurrentPage(); });
  server_.on("/wifi", HTTP_POST, [this]() { submitWifi(); });
  server_.on("/mqtt", HTTP_POST, [this]() { submitMqtt(); });
  server_.on("/unit", HTTP_POST, [this]() { submitUnit(); });
  server_.on("/back", HTTP_GET, [this]() {
    if (step_ == Step::Unit || step_ == Step::MqttTesting) {
      step_ = Step::Mqtt;
    } else if (mode_ == Mode::Full && (step_ == Step::Mqtt || step_ == Step::WifiTesting)) {
      step_ = Step::Wifi;  // only Full mode has a step 1 to go back to
    }
    error_ = nullptr;
    redirectToCurrentPage();
  });
  // Every other address, including each phone's captive-portal check, is sent
  // to the current step.
  server_.onNotFound([this]() { redirectToCurrentPage(); });
  server_.begin();

  active_ = true;
  if (mode_ == Mode::Full) {
    Serial.printf("Setup: network \"%s\", password %s, page %s\n", apSsid_, apPassword_, pageUrl_);
  } else {
    Serial.printf("Setup: broker settings only, page %s\n", pageUrl_);
  }
}

void Provisioning::end() {
  if (!active_) return;
  active_ = false;
  server_.stop();
  if (mode_ == Mode::Full) {
    dns_.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
  }
  Serial.println("Setup: closed");
}

const char* Provisioning::progress() const {
  const bool full = mode_ == Mode::Full;
  switch (step_) {
    case Step::Wifi:        return "Step 1 of 3: Wi-Fi";
    case Step::WifiTesting: return "Connecting to Wi-Fi...";
    case Step::Mqtt:        return full ? "Step 2 of 3: MQTT" : "Step 1 of 2: MQTT";
    case Step::MqttTesting: return "Connecting to the broker...";
    case Step::Unit:        return full ? "Step 3 of 3: mini-split" : "Step 2 of 2: mini-split";
    case Step::Done:        return "Setup complete";
  }
  return "";
}

const char* Provisioning::stepLabel() const {
  const bool full = mode_ == Mode::Full;
  switch (step_) {
    case Step::Wifi:
    case Step::WifiTesting: return "Step 1 of 3";
    case Step::Mqtt:
    case Step::MqttTesting: return full ? "Step 2 of 3" : "Step 1 of 2";
    case Step::Unit:        return full ? "Step 3 of 3" : "Step 2 of 2";
    case Step::Done:        return "Done";
  }
  return "";
}

// ---------------------------------------------------------------------------
// Watching the tests
// ---------------------------------------------------------------------------

void Provisioning::tick(unsigned long nowMs) {
  // Before handling requests: a test started by one of them stamps this same
  // time, so it can't appear to have started later than "now".
  nowMs_ = nowMs;

  if (mode_ == Mode::Full) dns_.processNextRequest();  // no access point, no captive portal
  server_.handleClient();

  if (step_ == Step::WifiTesting) {
    // WiFi.status() lags WiFi.begin(), and until it catches up it can still
    // report the previous attempt — including a connection to the network the
    // panel was on before setup, which would pass a password we never tried.
    if (nowMs_ - testStartedMs_ < SETUP_WIFI_TEST_SETTLE_MS) return;

    switch (WiFi.status()) {
      case WL_CONNECTED:
        Serial.printf("Setup: Wi-Fi test connected, IP %s\n", WiFi.localIP().toString().c_str());
        step_ = Step::Mqtt;
        error_ = nullptr;
        break;
      case WL_NO_SSID_AVAIL:
        failStep(Step::Wifi, "That network wasn't found. Check the name, and that it's 2.4GHz.");
        break;
      case WL_CONNECT_FAILED:
        failStep(Step::Wifi, "The network wouldn't accept that password.");
        break;
      default:
        if (nowMs_ - testStartedMs_ >= SETUP_WIFI_TEST_TIMEOUT_MS) {
          Serial.printf("Setup: Wi-Fi test gave up after %lu ms (status %d)\n",
                        nowMs_ - testStartedMs_, WiFi.status());
          WiFi.disconnect();
          failStep(Step::Wifi, "Couldn't connect in time. Check the password and try again.");
        }
    }
    return;
  }

  if (step_ == Step::MqttTesting) {
    if (mqtt_.connected()) {
      Serial.println("Setup: broker test connected");
      step_ = Step::Unit;
      error_ = nullptr;
    } else if (mqtt_.loginRefused()) {
      mqtt_.stop();
      failStep(Step::Mqtt, "The broker refused that username and password.");
    } else if (nowMs_ - testStartedMs_ >= SETUP_MQTT_TEST_TIMEOUT_MS) {
      Serial.printf("Setup: broker test gave up after %lu ms\n", nowMs_ - testStartedMs_);
      mqtt_.stop();
      failStep(Step::Mqtt, "Couldn't reach the broker. Check the address and port.");
    }
  }
}

void Provisioning::failStep(Step step, const char* error) {
  step_ = step;
  error_ = error;
  Serial.printf("Setup: %s\n", error);
}

void Provisioning::startWifiTest() {
  step_ = Step::WifiTesting;
  error_ = nullptr;
  testStartedMs_ = nowMs_;
  WiFi.setHostname(draft_.hostname);  // must be set before connecting
  WiFi.begin(draft_.wifiSsid, draft_.wifiPassword);
  Serial.printf("Setup: testing network \"%s\" as \"%s\"\n", draft_.wifiSsid, draft_.hostname);
}

void Provisioning::startMqttTest() {
  step_ = Step::MqttTesting;
  error_ = nullptr;
  testStartedMs_ = nowMs_;
  mqtt_.stop();  // drop an earlier attempt, or the one from saved settings
  mqtt_.begin(draft_.mqttHost, draft_.mqttPort, draft_.mqttUser, draft_.mqttPassword,
              draft_.hostname);
}

// ---------------------------------------------------------------------------
// The pages
// ---------------------------------------------------------------------------

void Provisioning::redirectToCurrentPage() {
  server_.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/");
  server_.send(302, "text/plain", "");
}

void Provisioning::sendCurrentPage() {
  String page;
  switch (step_) {
    case Step::Wifi: {
      page = pageTop(stepLabel(), false) + "<h1>Wi-Fi</h1>";
      if (error_) page += "<p class=error>" + escapeHtml(error_) + "</p>";
      page += "<form method=POST action=/wifi>"
              "<label>Network (2.4GHz)</label>"
              "<input name=ssid list=nets required value=\"" + escapeHtml(draft_.wifiSsid) + "\">"
              "<datalist id=nets>" + networkOptions_ + "</datalist>"
              "<label>Password</label>"
              "<input name=wifipass type=password placeholder=\"" +
              // Only offer to keep a password that exists, so a blank field
              // can't quietly save an empty one.
              String(draft_.wifiPassword[0] ? "leave blank to keep the saved one" : "required") +
              "\"><label>Name for this panel (optional)</label>"
              "<input name=hostname placeholder=\"" DEFAULT_HOSTNAME "\" value=\"" +
              escapeHtml(draft_.hostname) + "\">"
              "<p class=hint>What the panel calls itself on your network: the name in your "
              "router's device list, and &lt;name&gt;.local. Not the mini-split's name.</p>"
              "<button type=submit>Next</button></form>";
      break;
    }

    case Step::WifiTesting:
      page = pageTop(stepLabel(), true) + "<h1>Connecting...</h1>"
             "<p class=hint>Joining \"" + escapeHtml(draft_.wifiSsid) +
             "\". This page reloads by itself.</p>"
             "<p class=hint>If it stops loading, rejoin \"" + escapeHtml(apSsid_) +
             "\" — the panel changes Wi-Fi channel as it connects, which can drop your phone for "
             "a moment.</p><p><a href=/back>Back</a></p>";
      break;

    case Step::Mqtt: {
      page = pageTop(stepLabel(), false) + "<h1>MQTT</h1>";
      if (error_) page += "<p class=error>" + escapeHtml(error_) + "</p>";
      page += "<p class=hint>Wi-Fi is connected. Now the broker Home Assistant uses.</p>"
              "<form method=POST action=/mqtt>"
              "<label>Broker</label>"
              "<input name=mqtthost required value=\"" + escapeHtml(draft_.mqttHost) + "\">"
              "<label>Port</label>"
              "<input name=mqttport type=number required value=\"" + String(draft_.mqttPort) + "\">"
              "<p class=hint>Usually homeassistant.local on port 1883. Not port 8123, which is "
              "HA's web interface.</p>"
              "<label>Username</label>"
              "<input name=mqttuser value=\"" + escapeHtml(draft_.mqttUser) + "\">"
              "<label>Password</label>"
              "<input name=mqttpass type=password placeholder=\"" +
              String(draft_.mqttPassword[0] ? "leave blank to keep the saved one" : "required") +
              "\"><p class=hint>The broker login your mini-splits use. Leave both blank only if "
              "your broker allows anonymous clients.</p>"
              "<button type=submit>Next</button></form>";
      // Nothing to go back to when only the broker is being changed.
      if (mode_ == Mode::Full) page += "<p><a href=/back>Back</a></p>";
      break;
    }

    case Step::MqttTesting:
      page = pageTop(stepLabel(), true) + "<h1>Connecting...</h1>"
             "<p class=hint>Reaching " + escapeHtml(draft_.mqttHost) +
             ". This page reloads by itself.</p><p><a href=/back>Back</a></p>";
      break;

    case Step::Unit: {
      const int count = miniSplit_.unitCount();
      // The list is whatever Home Assistant has advertised on the discovery
      // topic since the broker connected, which takes a moment to arrive.
      page = pageTop(stepLabel(), count == 0) + "<h1>Mini-split</h1>";
      if (count == 0) {
        page += "<p class=hint>Waiting for Home Assistant to say which mini-splits exist. This "
                "page reloads by itself.</p>";
      } else {
        page += "<p class=hint>Which unit does this panel control?</p>"
                "<form class=units method=POST action=/unit>";
        for (int i = 0; i < count; i++) {
          const String name = escapeHtml(miniSplit_.unitNames()[i]);
          page += "<button type=submit name=unit value=\"" + name + "\">" + name + "</button>";
        }
        page += "</form>";
      }
      page += "<form method=POST action=/unit><button type=submit name=unit value=\"\">"
              "Skip, I'll pick on the panel</button></form>"
              "<p><a href=/back>Back</a></p>";
      break;
    }

    case Step::Done:
      page = pageTop(stepLabel(), false) + "<h1>All set</h1>"
             "<p class=hint>The panel is restarting" +
             String(mode_ == Mode::Full ? ". This setup network is about to disappear." : ".") +
             "</p>";
      break;
  }

  server_.send(200, "text/html", page + "</body></html>");
}

void Provisioning::submitWifi() {
  const String ssid = server_.arg("ssid");
  if (ssid.isEmpty()) {
    failStep(Step::Wifi, "A network name is needed.");
    redirectToCurrentPage();
    return;
  }

  snprintf(draft_.wifiSsid, sizeof(draft_.wifiSsid), "%s", ssid.c_str());

  // A blank password field means "keep the saved one", so a password can't be
  // wiped by resubmitting the form (the browser never receives it).
  const String password = server_.arg("wifipass");
  if (!password.isEmpty()) {
    snprintf(draft_.wifiPassword, sizeof(draft_.wifiPassword), "%s", password.c_str());
  }

  // The panel's name is optional; blank keeps whatever it had.
  const String hostname = server_.arg("hostname");
  if (!hostname.isEmpty()) {
    snprintf(draft_.hostname, sizeof(draft_.hostname), "%s", hostname.c_str());
  }

  startWifiTest();
  redirectToCurrentPage();
}

void Provisioning::submitMqtt() {
  const String host = server_.arg("mqtthost");
  const int port = server_.arg("mqttport").toInt();
  if (host.isEmpty() || port <= 0 || port > 65535) {
    failStep(Step::Mqtt, "A broker address and a port between 1 and 65535 are needed.");
    redirectToCurrentPage();
    return;
  }

  snprintf(draft_.mqttHost, sizeof(draft_.mqttHost), "%s", host.c_str());
  draft_.mqttPort = static_cast<uint16_t>(port);
  snprintf(draft_.mqttUser, sizeof(draft_.mqttUser), "%s", server_.arg("mqttuser").c_str());

  const String password = server_.arg("mqttpass");
  if (!password.isEmpty()) {
    snprintf(draft_.mqttPassword, sizeof(draft_.mqttPassword), "%s", password.c_str());
  }

  startMqttTest();
  redirectToCurrentPage();
}

void Provisioning::submitUnit() {
  snprintf(draft_.unitName, sizeof(draft_.unitName), "%s", server_.arg("unit").c_str());
  step_ = Step::Done;
  error_ = nullptr;

  sendCurrentPage();  // the phone sees "All set" before the panel restarts

  Serial.printf("Setup: saved network \"%s\", panel name \"%s\", broker %s:%u as \"%s\", unit "
                "\"%s\"\n",
                draft_.wifiSsid, draft_.hostname, draft_.mqttHost, draft_.mqttPort, draft_.mqttUser,
                draft_.unitName);
  if (onFinished_) onFinished_(context_, draft_);
}
