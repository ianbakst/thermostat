#pragma once
//
// Hardware configuration and tunables for the mini-split's controller
// (Seeed XIAO ESP32-C3).
//
// These are `constexpr`, not `#define`: they carry a type, they respect scope,
// and they show up in a debugger.
//
// Nothing private lives here. The network, its password, the broker login and
// this unit's name are entered during setup and saved in flash.
//

#include <cstdint>

// ---------------------------------------------------------------------------
// CN105: the connector on the indoor unit's control board
// ---------------------------------------------------------------------------
// The XIAO's USB-C port is the C3's built-in USB, so Serial (the logs) goes
// there and this hardware UART is free for the heat pump. On the ESP8266
// boards the two shared one port, which is why those couldn't be watched while
// plugged into a unit.
//
// CN105 pin 4 is the unit's transmit line and goes to our RX; pin 5 is its
// receive line and comes from our TX. Pin 2 is ground, pin 3 is the 5V the
// board runs from, pin 1 is 12V and unused. The signal lines are 5V, so
// there's a divider on RX; measure your own unit before wiring (see README).
constexpr int8_t CN105_RX_PIN = 20;  // D7 on the XIAO's silkscreen
constexpr int8_t CN105_TX_PIN = 21;  // D6
constexpr int CN105_BAUD = 2400;     // 8E1, which the HeatPump library sets up

// How often to ask the unit for its state. It answers in a few tens of
// milliseconds, and nothing about a room changes fast, so this is gentle on a
// 2400-baud link that also has to carry our commands.
constexpr unsigned long HP_SYNC_INTERVAL_MS = 1'000;

// Reconnecting to the indoor unit, which is unplugged, powered down or simply
// not answering yet. The wait doubles after each failure, up to the maximum,
// and then keeps trying forever at that interval: a unit that comes back
// hours later is picked up without anyone power-cycling this board.
constexpr unsigned long HP_RETRY_INTERVAL_MS = 1'000;
constexpr unsigned long HP_RETRY_MAX_MS = 15UL * 60 * 1'000;  // 15 minutes

// The setpoints every unit accepts, in °C. Newer units take half degrees and
// go down to 10 in heat, but an older one handed anything outside 16-31 sets
// 31 — the library falls back to the first entry of its table — so a request
// for 50°F would heat the room to 88°F. Every setpoint is clamped to this.
constexpr float HP_MIN_SETPOINT_C = 16;
constexpr float HP_MAX_SETPOINT_C = 31;

// ---------------------------------------------------------------------------
// Setup mode: the controller's own Wi-Fi network and its setup page
// ---------------------------------------------------------------------------
// The network is open (see net/provisioning.h for why) and named this plus
// the end of the board's MAC address, so two being set up at once are told
// apart.
constexpr const char* SETUP_AP_PREFIX = "Minisplit-";

// Holding the XIAO's BOOT button this long, while the controller is running,
// restarts it into setup mode. It has to be while running: the button is on
// GPIO9, which the chip reads at power-up to decide whether to run this
// firmware or wait to be flashed, so held through a reset it gets the latter.
constexpr uint8_t SETUP_BUTTON_PIN = 9;  // pulled to ground while pressed
constexpr unsigned long SETUP_BUTTON_HOLD_MS = 5'000;

// ---------------------------------------------------------------------------
// Wi-Fi and MQTT
// ---------------------------------------------------------------------------
constexpr const char* DEFAULT_UNIT_NAME = "minisplit";  // until setup gives it a real one
constexpr unsigned long WIFI_RETRY_INTERVAL_MS = 30'000;  // fresh attempt after this long offline

constexpr const char* DEFAULT_MQTT_HOST = "homeassistant.local";  // what the form starts with
constexpr uint16_t DEFAULT_MQTT_PORT = 1883;
constexpr unsigned long MQTT_RETRY_INTERVAL_MS = 5'000;  // wait between connection attempts
constexpr int MQTT_KEEPALIVE_S = 30;  // the broker drops us after 1.5x this in silence
// Big enough for the Home Assistant discovery config, which is the largest
// thing this controller sends by a wide margin: about 1.5KB with a 32-letter
// name, the longest the setup page takes.
constexpr int MQTT_BUFFER_SIZE = 2048;

// How often `state` goes out when nothing has changed. A change goes at once;
// this is the 30 seconds mitsubishi2mqtt used, which the protocol promises,
// so a panel or HA that missed one doesn't stay wrong for long.
constexpr unsigned long MQTT_STATE_INTERVAL_MS = 30'000;

// The control API's WebSocket (net/direct_api.h). A phone that locks its
// screen leaves its connection open without a word, so each client is pinged
// this often and dropped after missing this many replies in a row.
constexpr unsigned long DIRECT_API_PING_INTERVAL_MS = 15'000;
constexpr unsigned long DIRECT_API_PONG_TIMEOUT_MS = 5'000;
constexpr uint8_t DIRECT_API_MISSED_PONGS = 2;

// ---------------------------------------------------------------------------
// What this unit tells Home Assistant and the panels about itself
// ---------------------------------------------------------------------------
// Defaults for the setup form. The heat pump always works in °C; this is the
// unit everything above it sees, and a panel reflects it rather than
// converting. 50-90°F in steps of 1 is what these units are set to here.
constexpr bool DEFAULT_USE_FAHRENHEIT = true;
constexpr float DEFAULT_MIN_TEMP = 50;
constexpr float DEFAULT_MAX_TEMP = 90;
constexpr float DEFAULT_TEMP_STEP = 1;

// ---------------------------------------------------------------------------
// Firmware updates (OTA)
// ---------------------------------------------------------------------------
// What this build calls itself, injected by tools/firmware_version.py from the
// git tag (or from the version the release workflow made up). A macro, not
// constexpr: a build with no git history to describe falls back to the value
// below, and it gets pasted into string literals.
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif
