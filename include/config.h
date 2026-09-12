#pragma once
//
// Hardware configuration for the Waveshare ESP32-C6-Touch-LCD-1.47.
//
// The display, touch controller and ESP32-C6 are all on one board, so these
// pins are fixed by its wiring. Source:
// https://docs.waveshare.com/ESP32-C6-Touch-LCD-1.47
//

// ---------------------------------------------------------------------------
// Display: 1.47" IPS, 172x320, JD9853 controller over SPI
// ---------------------------------------------------------------------------
#define TFT_SCLK_PIN      1
#define TFT_MOSI_PIN      2
#define TFT_CS_PIN        14
#define TFT_DC_PIN        15
#define TFT_RST_PIN       22
#define TFT_BL_PIN        23       // backlight, active high, PWM dimmed
#define TFT_SPI_FREQ      80000000 // Waveshare's own demo runs this panel at 80MHz

#define TFT_PANEL_WIDTH   172      // native (portrait) resolution
#define TFT_PANEL_HEIGHT  320
#define TFT_COL_OFFSET    34       // the 172px panel sits in the middle of
                                   // the controller's 240px-wide address space

// Backlight turns off after this long without a touch; a tap wakes it
// (that tap doesn't press anything).
#define DISPLAY_SLEEP_TIMEOUT_MS 15000UL

// Backlight level while awake (0-255), set on the settings page and saved in
// flash. The minimum is what the slider's low end means: still visible in a
// dark room, never off.
#define DEFAULT_BRIGHTNESS 255
#define BRIGHTNESS_MIN     20

// 0 = portrait (USB at the bottom, as in Waveshare's demos), 1-3 rotate in
// 90° steps. Only 0 has been confirmed on this board so far.
#define DISPLAY_ROTATION  0

// ---------------------------------------------------------------------------
// Touch: AXS5106L capacitive controller over I2C
// (the QMI8658 IMU shares this I2C bus)
// ---------------------------------------------------------------------------
#define TOUCH_SDA_PIN     18
#define TOUCH_SCL_PIN     19
#define TOUCH_RST_PIN     20
#define TOUCH_INT_PIN     21       // unused for now: touch is polled
#define TOUCH_I2C_ADDR    0x63

// ---------------------------------------------------------------------------
// Other onboard hardware (not used yet)
// ---------------------------------------------------------------------------
#define SD_MISO_PIN       3        // TF card shares SCLK/MOSI with the display
#define SD_CS_PIN         4
#define IMU_INT1_PIN      5
#define IMU_INT2_PIN      6
#define BOOT_BUTTON_PIN   8
#define BATTERY_ADC_PIN   0

// ---------------------------------------------------------------------------
// Wi-Fi
// ---------------------------------------------------------------------------
// The network, its password, the panel's name and the broker login are all
// entered on the setup page and saved in flash (see src/net/provisioning.h);
// nothing private is compiled into the firmware. The DEFAULT_* values below
// are only what the form starts out with.
#define DEFAULT_HOSTNAME       "thermostat"  // name in your router's list, and <name>.local
#define WIFI_RETRY_INTERVAL_MS 30000UL       // start a fresh attempt after this long offline
#define WIFI_STATUS_REFRESH_MS 5000UL        // how often the settings page's signal updates

// ---------------------------------------------------------------------------
// Setup mode: the panel's own Wi-Fi network and setup page
// ---------------------------------------------------------------------------
#define SETUP_AP_PREFIX        "Thermostat-"  // plus the end of the MAC address
#define SETUP_AP_PASSWORD_LEN  8              // fresh random one each time; shown on the panel
#define SETUP_RESTART_DELAY_MS 2000UL         // lets the phone load the "saved" page first

// Each setup step tries what it collected before moving on. How long to wait
// before calling the attempt failed.
#define SETUP_WIFI_TEST_TIMEOUT_MS 20000UL
#define SETUP_MQTT_TEST_TIMEOUT_MS 15000UL

// How long to let the radio settle before believing WiFi.status(): just after
// WiFi.begin() it can still report the last attempt's result, including the
// network the panel was on before setup started.
#define SETUP_WIFI_TEST_SETTLE_MS  1500UL

// ---------------------------------------------------------------------------
// MQTT broker
// ---------------------------------------------------------------------------
// Home Assistant's Mosquitto broker. Not port 8123: that's HA's web UI/API.
#define DEFAULT_MQTT_HOST      "homeassistant.local"
#define DEFAULT_MQTT_PORT      1883
#define MQTT_RETRY_INTERVAL_MS 5000UL  // wait between connection attempts
#define MQTT_KEEPALIVE_S       30      // the broker drops us after 1.5x this with no contact
// Largest message we can receive. The biggest one we use is the mini-split's
// Home Assistant discovery config, ~2.4KB.
#define MQTT_BUFFER_SIZE       4096

// ---------------------------------------------------------------------------
// Mini-split
// ---------------------------------------------------------------------------
// Which unit this panel controls is picked on the panel, from the ones Home
// Assistant advertises, and saved in flash. Its temperature unit, setpoint
// limits and step come from the unit itself.
#define MINI_SPLIT_TOPIC_PREFIX  "mitsubishi2mqtt"
#define HA_DISCOVERY_PREFIX      "homeassistant"
#define MINI_SPLIT_MAX_UNITS     12  // how many discovered units the list holds

// +/- taps are sent once they pause for this long, so several quick taps
// make one change (and one beep from the unit), not one per tap.
#define SETPOINT_SEND_DELAY_MS   1000UL

// After a change is sent, reports still showing the old value are ignored
// for up to this long, so the screen doesn't jump back while the unit
// catches up.
#define PENDING_HOLD_MS          5000UL

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
#define LVGL_LOOP_DELAY_MS 5UL
#define PICKER_TIMEOUT_MS  10000UL  // mode/fan pickers close if nothing is chosen
