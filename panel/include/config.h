#pragma once
//
// Hardware configuration and tunables for the Waveshare ESP32-C6-Touch-LCD-1.47.
//
// The display, touch controller and ESP32-C6 are all on one board, so the pins
// are fixed by its wiring. Source:
// https://docs.waveshare.com/ESP32-C6-Touch-LCD-1.47
//
// These are `constexpr`, not `#define`: they carry a type, they respect scope,
// and they show up in a debugger. The exceptions are marked below — they're
// pasted into string literals, which only a macro can do.
//
// Nothing private lives here. The network, its password, the panel's name and
// the broker login are entered on the setup page and saved in flash (see
// src/net/provisioning.h); the DEFAULT_* values are only what the form starts
// out with.
//

#include <cstdint>

// ---------------------------------------------------------------------------
// Display: 1.47" IPS, 172x320, JD9853 controller over SPI
// ---------------------------------------------------------------------------
constexpr int8_t TFT_SCLK_PIN = 1;
constexpr int8_t TFT_MOSI_PIN = 2;
constexpr int8_t TFT_CS_PIN = 14;
constexpr int8_t TFT_DC_PIN = 15;
constexpr int8_t TFT_RST_PIN = 22;
constexpr int8_t TFT_BL_PIN = 23;               // backlight, active high, PWM dimmed
constexpr uint32_t TFT_SPI_FREQ = 80'000'000;   // Waveshare's own demo runs this panel at 80MHz

constexpr int16_t TFT_PANEL_WIDTH = 172;        // native (portrait) resolution
constexpr int16_t TFT_PANEL_HEIGHT = 320;
constexpr int16_t TFT_COL_OFFSET = 34;          // the 172px panel sits in the middle of
                                                // the controller's 240px-wide address space

// 0 = portrait (USB at the bottom, as in Waveshare's demos), 1-3 rotate in
// 90° steps. Only 0 has been confirmed on this board so far.
constexpr uint8_t DISPLAY_ROTATION = 0;

// Backlight turns off after this long without a touch; a tap wakes it
// (that tap doesn't press anything).
constexpr unsigned long DISPLAY_SLEEP_TIMEOUT_MS = 15'000;

// Backlight level while awake (0-255), set on the settings page and saved in
// flash. The minimum is what the slider's low end means: still visible in a
// dark room, never off.
constexpr uint8_t DEFAULT_BRIGHTNESS = 255;
constexpr uint8_t BRIGHTNESS_MIN = 20;

// ---------------------------------------------------------------------------
// Touch: AXS5106L capacitive controller over I2C
// (the QMI8658 IMU shares this I2C bus)
// ---------------------------------------------------------------------------
constexpr int8_t TOUCH_SDA_PIN = 18;
constexpr int8_t TOUCH_SCL_PIN = 19;
constexpr int8_t TOUCH_RST_PIN = 20;
constexpr int8_t TOUCH_INT_PIN = 21;            // pulled low when the controller has a new report
constexpr uint8_t TOUCH_I2C_ADDR = 0x63;

// ---------------------------------------------------------------------------
// Other onboard hardware (not used yet)
// ---------------------------------------------------------------------------
constexpr int8_t SD_MISO_PIN = 3;               // TF card shares SCLK/MOSI with the display
constexpr int8_t SD_CS_PIN = 4;
constexpr int8_t IMU_INT1_PIN = 5;
constexpr int8_t IMU_INT2_PIN = 6;
constexpr int8_t BOOT_BUTTON_PIN = 8;
constexpr int8_t BATTERY_ADC_PIN = 0;

// ---------------------------------------------------------------------------
// Wi-Fi
// ---------------------------------------------------------------------------
constexpr const char* DEFAULT_HOSTNAME = "thermostat";  // router's device list, and <name>.local
constexpr unsigned long WIFI_RETRY_INTERVAL_MS = 30'000;  // fresh attempt after this long offline
constexpr unsigned long WIFI_STATUS_REFRESH_MS = 5'000;   // how often the settings page refreshes

// ---------------------------------------------------------------------------
// Setup mode: the panel's own Wi-Fi network and setup pages
// ---------------------------------------------------------------------------
constexpr const char* SETUP_AP_PREFIX = "Thermostat-";  // plus the end of the MAC address
constexpr size_t SETUP_AP_PASSWORD_LEN = 8;      // fresh random one each time; shown on the panel
constexpr unsigned long SETUP_RESTART_DELAY_MS = 2'000;  // lets the phone load the last page first

// Each setup step tries what it collected before moving on. How long to wait
// before calling the attempt failed.
constexpr unsigned long SETUP_WIFI_TEST_TIMEOUT_MS = 20'000;
constexpr unsigned long SETUP_MQTT_TEST_TIMEOUT_MS = 15'000;

// How long to let the radio settle before believing WiFi.status(): just after
// WiFi.begin() it can still report the last attempt's result, including the
// network the panel was on before setup started.
constexpr unsigned long SETUP_WIFI_TEST_SETTLE_MS = 1'500;

// ---------------------------------------------------------------------------
// MQTT broker
// ---------------------------------------------------------------------------
// Home Assistant's Mosquitto broker. Not port 8123: that's HA's web UI/API.
constexpr const char* DEFAULT_MQTT_HOST = "homeassistant.local";
constexpr uint16_t DEFAULT_MQTT_PORT = 1883;
constexpr unsigned long MQTT_RETRY_INTERVAL_MS = 5'000;  // wait between connection attempts
constexpr int MQTT_KEEPALIVE_S = 30;             // the broker drops us after 1.5x this in silence
// Largest message we can receive. The biggest one we use is the mini-split's
// Home Assistant discovery config, ~2.4KB.
constexpr int MQTT_BUFFER_SIZE = 4096;

// ---------------------------------------------------------------------------
// Mini-split
// ---------------------------------------------------------------------------
// Which unit this panel controls is picked on the panel, from the ones Home
// Assistant advertises, and saved in flash. Its temperature unit, setpoint
// limits and step come from the unit itself.
//
// These two are macros, not constexpr (see the note at the top): they're
// pasted into topic string literals ("homeassistant" "/climate/+/config"),
// which needs the preprocessor.
#define MINI_SPLIT_TOPIC_PREFIX "mitsubishi2mqtt"
#define HA_DISCOVERY_PREFIX     "homeassistant"

constexpr int MINI_SPLIT_MAX_UNITS = 12;         // how many discovered units the list holds

// +/- taps are sent once they pause for this long, so several quick taps
// make one change (and one beep from the unit), not one per tap.
constexpr unsigned long SETPOINT_SEND_DELAY_MS = 1'000;

// After a change is sent, reports still showing the old value are ignored
// for up to this long, so the screen doesn't jump back while the unit
// catches up.
constexpr unsigned long PENDING_HOLD_MS = 5'000;

// ---------------------------------------------------------------------------
// Firmware updates (OTA)
// ---------------------------------------------------------------------------
// What this build calls itself, injected by tools/firmware_version.py from the
// git tag (or from the version the release workflow made up). The updater
// compares it against the version in the channel's manifest to decide whether
// there's anything to install. A macro, not constexpr: it's pasted into the
// User-Agent string literal, and a build with no git history to describe falls
// back to the value below.
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif

// Where each channel says what it has. Both are small JSON files (written by
// tools/make_manifest.py) attached to a GitHub release:
//
//   stable  the newest tagged release off main. "latest" is GitHub resolving
//           "the newest release that isn't a prerelease" for us.
//   beta    the rolling prerelease the `beta` tag is moved to on every push
//           to dev.
//
// These name the repo the firmware comes from, not anything about your home.
// The repo releases two firmwares, so each manifest says which device it is
// for: a panel reads panel-manifest.json, which points at panel-firmware.bin.
constexpr const char* OTA_STABLE_MANIFEST_URL =
    "https://github.com/ianbakst/thermostat/releases/latest/download/panel-manifest.json";
constexpr const char* OTA_BETA_MANIFEST_URL =
    "https://github.com/ianbakst/thermostat/releases/download/beta/panel-manifest.json";

constexpr unsigned long OTA_CHECK_INTERVAL_MS = 24UL * 60 * 60 * 1'000;  // once a day
constexpr unsigned long OTA_FIRST_CHECK_DELAY_MS = 60'000;   // after the network has settled
constexpr int OTA_HTTP_TIMEOUT_MS = 15'000;

// A release asset URL is answered with a redirect to GitHub's storage host,
// and the signed URL in that Location header — then in the request that
// follows it — runs to about a kilobyte. esp_http_client defaults both
// buffers to 512 bytes, which the redirect doesn't fit in: it gives up with
// "Out of buffer" rather than anything that names the real problem. These
// apply to the download as well, which takes the same road.
constexpr int OTA_HTTP_RX_BUFFER = 4'096;
constexpr int OTA_HTTP_TX_BUFFER = 2'048;
constexpr size_t OTA_MANIFEST_MAX_BYTES = 1'024;             // a manifest is a few hundred bytes

// A freshly installed image is on trial: the bootloader keeps the firmware it
// replaced until the new one has run this long with the network up. One that
// crashes on the way up, or that can't join Wi-Fi, is rolled back on the next
// boot rather than stranding a panel that's only reachable over the air.
constexpr unsigned long OTA_SELF_TEST_MS = 60'000;

// The download runs in a task of its own so the UI keeps drawing. The stack
// (in bytes, as ESP-IDF counts it) has to hold a TLS handshake against the
// certificate bundle, which is most of it; the priority matches the Arduino
// loop task, so neither starves the other. The task only exists while a check
// or a download is running.
constexpr uint32_t OTA_TASK_STACK = 12'288;
constexpr unsigned OTA_TASK_PRIORITY = 1;

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
constexpr unsigned long LVGL_LOOP_DELAY_MS = 5;
constexpr uint32_t PICKER_TIMEOUT_MS = 10'000;   // mode/fan pickers close if nothing is chosen
