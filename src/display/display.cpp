#include "display/display.h"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>

#include "config.h"
#include "display/jd9853_init.h"
#include "touch/axs5106l.h"

static Arduino_HWSPI bus(TFT_DC_PIN, TFT_CS_PIN, TFT_SCLK_PIN, TFT_MOSI_PIN);

// ips=true: Arduino_GFX sends "invert off" after the init sequence, and the
// IPS flag flips that to "invert on", which this panel needs.
static Arduino_ST7789 panel(&bus, TFT_RST_PIN, DISPLAY_ROTATION, true /* ips */,
                            TFT_PANEL_WIDTH, TFT_PANEL_HEIGHT,
                            TFT_COL_OFFSET, 0, TFT_COL_OFFSET, 0,
                            jd9853_init_operations, sizeof(jd9853_init_operations));

static Axs5106l touch;
static bool touchOk = false;

// Sleep: after DISPLAY_SLEEP_TIMEOUT_MS without a touch the backlight turns
// off. LVGL keeps running, so the screen is up to date when it wakes.
static uint8_t awakeBrightness = 255;
static bool asleep = false;
static bool sleepAllowed = true;
// Set when a touch wakes the display: that touch is hidden from LVGL until
// the finger lifts, so waking can't press whatever was under it.
static bool ignoreUntilRelease = false;

static void sleepDisplay() {
  asleep = true;
  ledcWrite(TFT_BL_PIN, 0);
  Serial.println("Display: sleep");
}

static void wakeDisplay() {
  asleep = false;
  ledcWrite(TFT_BL_PIN, awakeBrightness);
  Serial.println("Display: wake");
}

// LVGL draws into this buffer in strips, then flushCb copies each strip to
// the panel. A tenth of the screen is the usual size/speed tradeoff.
static uint8_t drawBuf[TFT_PANEL_WIDTH * TFT_PANEL_HEIGHT / 10 * 2];  // RGB565 = 2 bytes/px

static void flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* pxMap) {
  const int32_t w = lv_area_get_width(area);
  const int32_t h = lv_area_get_height(area);
  // LVGL's RGB565 buffer is native-endian uint16, which is what
  // draw16bitRGBBitmap expects.
  panel.draw16bitRGBBitmap(area->x1, area->y1,
                           static_cast<uint16_t*>(static_cast<void*>(pxMap)), w, h);
  lv_display_flush_ready(disp);
}

static void touchReadCb(lv_indev_t* indev, lv_indev_data_t* data) {
  int32_t x, y;
  const bool touching = touchOk && touch.read(x, y);

  if (touching && asleep) {
    wakeDisplay();
    ignoreUntilRelease = true;
  }

  if (ignoreUntilRelease) {
    if (touching) {
      // LVGL only counts reported presses as activity, so tell it directly
      // that someone is here; otherwise it would go straight back to sleep.
      lv_display_trigger_activity(lv_indev_get_display(indev));
    } else {
      ignoreUntilRelease = false;
    }
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  if (touching) {
    data->point.x = x;
    data->point.y = y;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

static void logCb(lv_log_level_t, const char* msg) { Serial.print(msg); }

void Display::begin() {
  if (!panel.begin(TFT_SPI_FREQ)) {
    Serial.println("Display: panel init failed");
  }
  panel.fillScreen(RGB565_BLACK);

  ledcAttach(TFT_BL_PIN, 5000, 8);
  setBrightness(255);

  touchOk = touch.begin(DISPLAY_ROTATION);

  lv_init();
  lv_log_register_print_cb(logCb);
  lv_tick_set_cb([]() -> uint32_t { return millis(); });

  // Size after rotation, e.g. 172x320 portrait or 320x172 landscape.
  const int32_t width = panel.width();
  const int32_t height = panel.height();

  lv_display_t* disp = lv_display_create(width, height);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(disp, flushCb);
  lv_display_set_buffers(disp, drawBuf, nullptr, sizeof(drawBuf),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t* indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchReadCb);

  Serial.printf("Display: %dx%d, rotation %d\n", width, height, DISPLAY_ROTATION);
}

void Display::tick() {
  lv_timer_handler();

  // Time since the last touch, as tracked by LVGL.
  if (sleepAllowed && !asleep && lv_display_get_inactive_time(nullptr) >= DISPLAY_SLEEP_TIMEOUT_MS) {
    sleepDisplay();
  }
}

void Display::setSleepAllowed(bool allowed) {
  sleepAllowed = allowed;
  if (!allowed && asleep) wakeDisplay();
}

void Display::setBrightness(uint8_t level) {
  awakeBrightness = level;
  if (!asleep) ledcWrite(TFT_BL_PIN, level);
}
