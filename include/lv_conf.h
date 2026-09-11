// LVGL configuration.
//
// Only the options that differ from LVGL's defaults are set here; everything
// else falls back to lv_conf_internal.h. The full annotated template is at
// .pio/libdeps/esp32c6-touch-lcd/lvgl/lv_conf_template.h after the first build.

#ifndef LV_CONF_H
#define LV_CONF_H

// ---------------------------------------------------------------------------
// Color / memory
// ---------------------------------------------------------------------------
#define LV_COLOR_DEPTH 16

#define LV_USE_STDLIB_MALLOC LV_STDLIB_BUILTIN
#define LV_MEM_SIZE (64 * 1024U)

// How often LVGL polls touch and redraws the screen. It only redraws when
// something changed, so a short period costs little while idle but cuts the
// delay between touching and seeing a response.
#define LV_DEF_REFR_PERIOD 16  // ms, ~60 fps
#define LV_DPI_DEF 130

#define LV_USE_OS LV_OS_NONE  // LVGL is only touched from the Arduino loop task

// ---------------------------------------------------------------------------
// Logging (routed to Serial in display.cpp)
// ---------------------------------------------------------------------------
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 0

// ---------------------------------------------------------------------------
// Performance monitor: a small overlay in the bottom-right corner showing
//   "<fps> FPS, <cpu>% CPU"
//   "<total>ms (<render> | <flush>)"
// where render = LVGL drawing pixels, flush = sending them over SPI.
// Set LV_USE_PERF_MONITOR to 1 to show it (it covers part of the fan button).
// ---------------------------------------------------------------------------
#define LV_USE_SYSMON 1
#define LV_USE_PERF_MONITOR 0
#define LV_USE_PERF_MONITOR_POS LV_ALIGN_BOTTOM_RIGHT

// ---------------------------------------------------------------------------
// Fonts
// ---------------------------------------------------------------------------
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#endif  // LV_CONF_H
