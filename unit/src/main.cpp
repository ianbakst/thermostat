// The mini-split's controller: it sits inside the indoor unit, speaks CN105 to
// its control board, and publishes that state over MQTT for the panel and
// Home Assistant.
//
// It reads the heat pump, joins the network it was told about during setup,
// and offers the unit two ways: on the broker, where HA finds it by itself
// and the panels already look, and on pages on the home network. Changes from
// either go to the heat pump the same way.
//
// Everything lives in its own class; this file only wires them together.

#include <Arduino.h>

#include "climate/heat_pump.h"
#include "config.h"
#include "input/setup_button.h"
#include "net/direct_api.h"
#include "net/mqtt_api.h"
#include "net/mqtt_connection.h"
#include "net/provisioning.h"
#include "net/web_pages.h"
#include "net/wifi_connection.h"
#include "storage/settings_store.h"

static HeatPumpLink heatPump;
static SettingsStore store;
static UnitSettings settings;
static Provisioning provisioning;
static SetupButton setupButton;
static WifiConnection wifi;
static MqttConnection mqtt;
static MqttApi mqttApi;
static WebPages webPages;
static DirectApi directApi;

static void logState(const HeatPumpState& state) {
  char text[192];
  state.describe(text, sizeof(text));
  Serial.printf("Heat pump: %s\n", text);
}

void setup() {
  // With USB attached and nothing reading, each write otherwise blocks for
  // about two seconds, which would stall the heat pump link.
  Serial.setTxTimeoutMs(0);
  Serial.begin(115200);

  Serial.printf("\nMini-split controller %s\n", FIRMWARE_VERSION);

  store.begin();
  settings = store.load();

  // Taken whether it's needed or not, so it only ever applies to the boot
  // right after the button was held.
  const bool setupRequested = Provisioning::takeRequest();

  if (!settings.hasNetwork() || setupRequested) {
    // The heat pump is left alone during setup. Connecting to it blocks for
    // a second or two per attempt, and with nothing on the other end — a
    // board on a bench, or one whose CN105 lead isn't plugged in yet — those
    // stalls would make the setup page time out. Setup ends in a restart, so
    // the link starts then.
    Serial.println(setupRequested ? "Setup: asked for with the BOOT button, starting setup mode"
                                  : "Setup: no network saved, starting setup mode");
    provisioning.begin(store, settings);
  } else {
    heatPump.begin();
    setupButton.begin();
    Serial.printf("Unit \"%s\", reporting in %s\n", settings.name,
                  settings.useFahrenheit ? "°F" : "°C");
    wifi.begin(settings.wifiSsid, settings.wifiPassword, settings.name, WIFI_RETRY_INTERVAL_MS);
  }
}

void loop() {
  const unsigned long now = millis();

  if (provisioning.active()) {
    provisioning.tick();
    return;  // the radio, and the loop, belong to setup until it restarts
  }

  if (setupButton.tick(now)) {
    Serial.println("Setup: BOOT button held, restarting into setup mode");
    Provisioning::requestAtNextBoot();
    ESP.restart();
  }

  if (heatPump.tick(now)) {
    logState(heatPump.state());
    directApi.publish();
    mqttApi.publish(now);
  }

  const bool wifiChanged = wifi.tick(now);

  // MQTT, the pages and the control API need the network, so they start when
  // Wi-Fi first connects. After that MQTT reconnects by itself, including
  // after Wi-Fi drops, and the other two are simply there whenever the
  // network is.
  if (wifiChanged && wifi.connected()) {
    // MqttApi adds the last will, since it owns the availability topic.
    const MqttConnection::Options options{
        .keepaliveS = MQTT_KEEPALIVE_S,
        .retryIntervalMs = MQTT_RETRY_INTERVAL_MS,
        .bufferSize = MQTT_BUFFER_SIZE,
    };
    mqttApi.begin(mqtt, heatPump, settings, options);
    webPages.begin(store, settings, heatPump, wifi, mqtt);
    directApi.begin(heatPump, settings);
  }

  mqttApi.tick(now);
  webPages.tick();
  directApi.tick();

  delay(10);
}
