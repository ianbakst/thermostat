// thermostat — in-wall touch panel for mini-split heat pumps
//
// Hardware: Waveshare ESP32-C6-Touch-LCD-1.47.
//
// This file only wires modules together and runs the loop. Each piece of
// hardware has its own module under src/.
//
// Nothing about the installation is compiled in: the network, its password,
// the panel's name and the broker login are entered on the setup page the
// panel serves over its own Wi-Fi network, and the mini-split is picked on
// screen. All of it is saved in flash.

#include <Arduino.h>

#include "climate/mini_split.h"
#include "config.h"
#include "display/display.h"
#include "net/mqtt_connection.h"
#include "net/ota_updater.h"
#include "net/provisioning.h"
#include "net/wifi_connection.h"
#include "storage/settings_store.h"
#include "ui/ui.h"

static Display display;
static Ui ui;
static SettingsStore settingsStore;
static WifiConnection wifi;
static MqttConnection mqtt;
static MiniSplit miniSplit(mqtt);  // the unit this panel controls
static Provisioning provisioning(mqtt, miniSplit);  // setup mode tests both
static OtaUpdater ota;

// Everything this panel was told about its installation. The network and MQTT
// modules keep pointers into it, so it lives as long as the program does.
static DeviceSettings settings;

// When to restart, after settings change (0 = not restarting). Saving the new
// settings and coming up fresh is simpler and more predictable than
// reconfiguring Wi-Fi, MQTT and its subscriptions while they're running.
static unsigned long restartAtMs = 0;

// This pass's time, shared by everything in it. The tap callbacks below run
// from LVGL inside display.tick(), which is the end of the same pass, so
// reading millis() there would stamp a time later than the one modules were
// ticked with — and an unsigned "now - then" would then underflow into a huge
// elapsed time. One clock per pass keeps that impossible.
static unsigned long loopNowMs = 0;

static void restartSoon(const char* why) {
  Serial.printf("Restarting: %s\n", why);
  ui.showRestarting();
  restartAtMs = millis() + SETUP_RESTART_DELAY_MS;
}

// ---------------------------------------------------------------------------
// What happens when the user taps a control: the change goes to the
// mini-split, and the screen shows it straight away.
// ---------------------------------------------------------------------------

static void setTargetTemp(float temp) {
  miniSplit.setTargetTemp(temp, loopNowMs);
  ui.show(miniSplit.state());
}

static void setMode(HvacMode mode) {
  miniSplit.setMode(mode, loopNowMs);
  ui.show(miniSplit.state());
}

static void setFanMode(FanMode fan) {
  miniSplit.setFanMode(fan, loopNowMs);
  ui.show(miniSplit.state());
}

static void setVane(VaneMode vane) {
  miniSplit.setVane(vane, loopNowMs);
  ui.show(miniSplit.state());
}

static void setWideVane(WideVaneMode wideVane) {
  miniSplit.setWideVane(wideVane, loopNowMs);
  ui.show(miniSplit.state());
}

// ---------------------------------------------------------------------------
// Setup mode and the mini-split choice
// ---------------------------------------------------------------------------

static void onSetupFinished(void*, const DeviceSettings& submitted) {
  settings = submitted;
  settingsStore.save(settings);
  restartSoon("setup complete");
}

static void startWifiSetup() {
  if (provisioning.active()) return;
  // Setup takes over the radio to try the network being entered, so the
  // saved-network retry has to stop fighting it.
  wifi.pause();
  provisioning.begin(Provisioning::Mode::Full, settings, onSetupFinished, nullptr);

  char password[48];
  snprintf(password, sizeof(password), "Password: %s", provisioning.apPassword());
  // The QR code has to stay readable long enough to scan and type from.
  display.setSleepAllowed(false);
  ui.showSetup(provisioning.joinQrData(), provisioning.apSsid(), password,
               "Scan to join, then the setup page opens by itself",
               settings.hasNetwork() /* cancellable: there's a network to go back to */);
  ui.showSetupProgress(provisioning.progress());
}

// Just the broker settings, over the network the panel is already on: the
// phone doesn't have to leave your Wi-Fi, and step 1 isn't asked again.
static void startBrokerSetup() {
  if (provisioning.active() || !wifi.connected()) return;
  provisioning.begin(Provisioning::Mode::BrokerOnly, settings, onSetupFinished, nullptr);
  display.setSleepAllowed(false);
  ui.showSetup(provisioning.pageUrl(), "On your network", provisioning.pageUrl(),
               "Scan to open the setup page, or type that address", true /* cancellable */);
  ui.showSetupProgress(provisioning.progress());
}

// Setup has by now taken over Wi-Fi and the broker connection, so the way back
// to normal operation is a restart rather than unpicking all of that.
static void cancelSetup() { restartSoon("setup cancelled"); }

static void openUnitList() {
  ui.showUnits(miniSplit.unitNames(), miniSplit.unitCount(), settings.unitName);
}

static void setUnit(const char* name) {
  settingsStore.saveUnitName(name);
  restartSoon("mini-split changed");
}

// ---------------------------------------------------------------------------
// Firmware updates
// ---------------------------------------------------------------------------

// These only ask: the updater does the work in a task of its own and loop()
// picks up the answer. Showing the status straight away is what turns the
// button into "Checking..." on the tap rather than a moment later.
static void checkForUpdate() {
  ota.check(loopNowMs);
  ui.showOta(ota.status(), settings.otaChannel);
}

static void installUpdate() {
  ota.install();
  ui.showOta(ota.status(), settings.otaChannel);
}

static void openChannelList() { ui.showChannels(settings.otaChannel); }

// Unlike the mini-split, this doesn't need a restart: nothing is connected to
// a channel, it's only where the next check looks. So look now.
static void setOtaChannel(OtaChannel channel) {
  settings.otaChannel = channel;
  settingsStore.saveOtaChannel(channel);
  ota.setChannel(channel);
  ota.check(loopNowMs);
  ui.showOta(ota.status(), settings.otaChannel);
}

// Dragging the slider follows the backlight live; only letting go writes it to
// flash, so a drag doesn't leave a trail of saves behind it.
static void setBrightness(uint8_t value, bool save) {
  display.setBrightness(value);
  if (!save) return;
  settings.brightness = value;
  settingsStore.saveBrightness(value);
  Serial.printf("Brightness: %u (saved)\n", value);
}

// ---------------------------------------------------------------------------

void setup() {
  // Serial runs over the C6's built-in USB. When a computer is attached but
  // nothing is reading the port (no serial monitor open), each write would
  // wait up to ~2s for it by default, freezing the UI on every log line.
  // With no timeout, output is dropped instead when nobody is listening; the
  // larger buffer keeps bursts like the startup log intact when someone is.
  Serial.setTxBufferSize(1024);
  Serial.setTxTimeoutMs(0);
  Serial.begin(115200);
  Serial.println();
  Serial.println("Thermostat starting");
  Serial.println("-------------------------------------");

  settingsStore.begin();
  settings = settingsStore.load();
  ota.begin(settings.otaChannel);

  display.begin(settings.brightness);
  ui.begin({setTargetTemp, setMode, setFanMode, setVane, setWideVane, startWifiSetup,
            startBrokerSetup, cancelSetup, openUnitList, setUnit, checkForUpdate, installUpdate,
            openChannelList, setOtaChannel, setBrightness});
  ui.showBrightness(settings.brightness);
  ui.show(miniSplit.state());

  // Registers the MQTT topics, including the one that lists the units to pick
  // from. Setup mode needs that list too, so this runs either way.
  miniSplit.begin(settings.unitName);

  if (!settings.hasNetwork()) {
    Serial.println("No network saved yet");
    startWifiSetup();
    return;
  }

  Serial.printf("Panel \"%s\", network \"%s\", broker %s:%u as \"%s\", unit \"%s\"\n",
                settings.hostname, settings.wifiSsid, settings.mqttHost, settings.mqttPort,
                settings.mqttUser, settings.unitName);

  // Connects in the background; the UI is already up and responsive.
  wifi.begin(settings.wifiSsid, settings.wifiPassword, settings.hostname);
}

void loop() {
  const unsigned long now = millis();
  loopNowMs = now;

  if (restartAtMs != 0 && now >= restartAtMs) ESP.restart();

  // Setup mode: serve the pages to whoever joined the panel's network, and
  // show on the panel how far they've got.
  if (provisioning.active()) {
    provisioning.tick(now);
    static const char* shownProgress = nullptr;
    if (provisioning.progress() != shownProgress) {  // always the same few strings
      shownProgress = provisioning.progress();
      ui.showSetupProgress(shownProgress);
    }
  }

  // Refresh the Wi-Fi status on the settings page when the connection comes
  // or goes, and every few seconds so the signal strength stays current.
  static unsigned long lastStatusMs = 0;
  const bool wifiChanged = wifi.tick(now);
  if (wifiChanged || now - lastStatusMs >= WIFI_STATUS_REFRESH_MS) {
    lastStatusMs = now;
    ui.showWifiStatus(wifi.status());

    char broker[80];
    snprintf(broker, sizeof(broker), "%s:%u", settings.mqttHost, settings.mqttPort);
    ui.showMqttStatus(broker, mqtt.connected(), mqtt.loginRefused());
  }

  // MQTT needs the network, so it starts when Wi-Fi first connects. After
  // that it reconnects by itself, including after Wi-Fi drops. While setup is
  // running it owns the broker connection, testing what's being typed.
  if (wifiChanged && wifi.connected() && !provisioning.active()) {
    mqtt.begin(settings.mqttHost, settings.mqttPort, settings.mqttUser, settings.mqttPassword,
               settings.hostname);
  }

  if (miniSplit.tick(now)) ui.show(miniSplit.state());

  // Firmware updates. Setup mode is off testing a network and a broker of its
  // own, so it keeps the radio to itself and the panel isn't offered an update
  // in the middle of being configured.
  const bool networkReady = wifi.connected() && !provisioning.active();
  if (ota.tick(now, networkReady)) {
    ui.showOta(ota.status(), settings.otaChannel);
    // A download takes a while and shows how far it's got, so the backlight
    // stays on for it — but only for that, since blocking sleep also wakes the
    // screen, and a daily check shouldn't light up a dark room. Setup mode is
    // already holding the backlight on for its QR code and would have it
    // turned back off here.
    if (!provisioning.active()) display.setSleepAllowed(!ota.installing());
    if (ota.status().state == OtaState::Installed) restartSoon("update installed");
  }

  display.tick();
  delay(LVGL_LOOP_DELAY_MS);
}
