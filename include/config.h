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
// Thermostat
// ---------------------------------------------------------------------------
// TempUnit::Fahrenheit or TempUnit::Celsius: the unit used until one is
// chosen on the settings page (that choice is saved in flash and wins from
// then on). Temperatures are stored in °C; the unit only changes what's
// displayed and the +/- step (1°F or 0.5°C).
#define DEFAULT_TEMP_UNIT     TempUnit::Fahrenheit

// Setpoint limits. Typical mini-split range (~61-88°F); Home Assistant
// reports the real limits, which we'll use from step 4.
#define TARGET_TEMP_MIN_C     16.0f
#define TARGET_TEMP_MAX_C     31.0f

// ---------------------------------------------------------------------------
// Wi-Fi (network name and password live in include/secrets.h, not in git)
// ---------------------------------------------------------------------------
#define WIFI_HOSTNAME          "thermostat"  // name shown in your router's device list
#define WIFI_RETRY_INTERVAL_MS 30000UL       // start a fresh attempt after this long offline
#define WIFI_STATUS_REFRESH_MS 5000UL        // how often the settings page's signal updates

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
#define LVGL_LOOP_DELAY_MS 5UL
#define PICKER_TIMEOUT_MS  10000UL  // mode/fan pickers close if nothing is chosen
