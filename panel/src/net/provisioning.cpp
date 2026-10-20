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

// A network name can contain characters that mean something in HTML.
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

    snprintf(apSsid_, sizeof(apSsid_), "%s%06llX", SETUP_AP_PREFIX,
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
  const MqttConnection::Options options{
      .keepaliveS = MQTT_KEEPALIVE_S,
      .retryIntervalMs = MQTT_RETRY_INTERVAL_MS,
      .bufferSize = MQTT_BUFFER_SIZE,
  };
  mqtt_.begin(draft_.mqttHost, draft_.mqttPort, draft_.mqttUser, draft_.mqttPassword,
              draft_.hostname, options);
}

// ---------------------------------------------------------------------------
// The pages
// ---------------------------------------------------------------------------

void Provisioning::redirectToCurrentPage() {
  // pageUrl_, not the access point's address: in BrokerOnly mode there is no
  // access point, and the pages are served on the panel's own network address.
  server_.sendHeader("Location", pageUrl_);
  server_.send(302, "text/plain", "");
}

setup_pages::Context Provisioning::pageContext() const {
  setup_pages::Context context{stepLabel(), draft_};
  context.error = error_;
  context.networks = &networkOptions_;
  context.apSsid = apSsid_;
  // Only Full mode has an earlier step; BrokerOnly starts at the broker.
  context.canGoBack = mode_ == Mode::Full;
  return context;
}

void Provisioning::sendCurrentPage() {
  const setup_pages::Context context = pageContext();

  String page;
  switch (step_) {
    case Step::Wifi:        page = setup_pages::wifiForm(context); break;
    // The waiting pages always offer Back: it's how a test in progress is
    // abandoned, whichever step started it.
    case Step::WifiTesting: page = setup_pages::wifiTesting(context); break;
    case Step::Mqtt:        page = setup_pages::mqttForm(context); break;
    case Step::MqttTesting: page = setup_pages::mqttTesting(context); break;
    case Step::Unit:
      page = setup_pages::unitChoice(context, miniSplit_.unitNames(), miniSplit_.unitCount());
      break;
    case Step::Done:        page = setup_pages::done(context, mode_ == Mode::Full); break;
  }

  server_.send(200, "text/html", page);
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
