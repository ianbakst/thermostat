#include "ui/settings_screen.h"

#include <cstdio>
#include <cstring>

#include "ui/fonts/fonts.h"
#include "ui/widgets.h"

static lv_color_t selectedColor() { return lv_palette_main(LV_PALETTE_BLUE); }

static lv_obj_t* makeCaption(lv_obj_t* parent, const char* text, int32_t y) {
  lv_obj_t* caption = lv_label_create(parent);
  lv_label_set_text(caption, text);
  lv_obj_set_style_text_font(caption, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(caption, lv_palette_main(LV_PALETTE_GREY), 0);
  lv_obj_set_pos(caption, 12, y);
  return caption;
}

// What a signal strength means in practice, matching the bars on the main
// screen's Wi-Fi icon.
static const char* signalQuality(int rssi) {
  switch (signalBars(rssi)) {
    case 4:  return "strong";
    case 3:  return "good";
    case 2:  return "weak";
    default: return "very weak";
  }
}

// A button showing one unit choice ("°F" / "°C").
static lv_obj_t* makeUnitButton(lv_obj_t* parent, TempUnit unit, int32_t x) {
  lv_obj_t* button = createFlatButton(parent, 74, 48);
  lv_obj_set_pos(button, x, 92);
  lv_obj_t* label = lv_label_create(button);
  lv_label_set_text(label, tempUnitSymbol(unit));
  lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
  lv_obj_center(label);
  return button;
}

void SettingsScreen::open(TempUnit unit, OnTempUnitSelected onTempUnitSelected, void* context) {
  if (overlay_) return;  // already open
  onTempUnitSelected_ = onTempUnitSelected;
  context_ = context;

  overlay_ = createOverlay();

  // Top-left: close, in the same spot as the settings button that opened
  // the page, with the title beside it.
  lv_obj_t* closeButton = createFlatButton(overlay_, 40, 40);
  lv_obj_align(closeButton, LV_ALIGN_TOP_LEFT, 8, 8);
  lv_obj_set_ext_click_area(closeButton, 8);
  lv_obj_center(createIcon(closeButton, ICON_CLOSE, &mdi_icons_24));
  lv_obj_add_event_cb(
      closeButton, [](lv_event_t* e) { static_cast<SettingsScreen*>(lv_event_get_user_data(e))->close(); },
      LV_EVENT_CLICKED, this);

  lv_obj_t* title = lv_label_create(overlay_);
  lv_label_set_text(title, "Settings");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 60, 17);

  // Units.
  makeCaption(overlay_, "Units", 70);

  fahrenheitButton_ = makeUnitButton(overlay_, TempUnit::Fahrenheit, 8);
  lv_obj_add_event_cb(
      fahrenheitButton_,
      [](lv_event_t* e) {
        static_cast<SettingsScreen*>(lv_event_get_user_data(e))->selectTempUnit(TempUnit::Fahrenheit);
      },
      LV_EVENT_CLICKED, this);

  celsiusButton_ = makeUnitButton(overlay_, TempUnit::Celsius, 90);
  lv_obj_add_event_cb(
      celsiusButton_,
      [](lv_event_t* e) {
        static_cast<SettingsScreen*>(lv_event_get_user_data(e))->selectTempUnit(TempUnit::Celsius);
      },
      LV_EVENT_CLICKED, this);

  // Wi-Fi status (read-only).
  makeCaption(overlay_, "Wi-Fi", 162);
  wifiLabel_ = lv_label_create(overlay_);
  lv_obj_set_style_text_font(wifiLabel_, &lv_font_montserrat_14, 0);
  lv_obj_set_width(wifiLabel_, 148);
  lv_label_set_long_mode(wifiLabel_, LV_LABEL_LONG_WRAP);
  lv_obj_set_pos(wifiLabel_, 12, 182);

  showTempUnit(unit);
  showWifiStatus(wifiStatus_);
}

void SettingsScreen::showTempUnit(TempUnit unit) {
  unit_ = unit;
  if (!overlay_) return;

  const bool fahrenheit = unit == TempUnit::Fahrenheit;
  lv_obj_set_style_bg_color(fahrenheitButton_, fahrenheit ? selectedColor() : buttonColor(), 0);
  lv_obj_set_style_bg_color(celsiusButton_, fahrenheit ? buttonColor() : selectedColor(), 0);
}

void SettingsScreen::showWifiStatus(const WifiStatus& status) {
  wifiStatus_ = status;
  if (!overlay_) return;

  char text[96];
  if (status.connected) {
    snprintf(text, sizeof(text), "%s\n%s\nSignal: %d dBm (%s)", status.ssid, status.ip, status.rssi,
             signalQuality(status.rssi));
  } else {
    snprintf(text, sizeof(text), "Connecting to %s...", status.ssid);
  }

  // Only touch the label when the text changes; setting it always redraws.
  if (strcmp(lv_label_get_text(wifiLabel_), text) != 0) lv_label_set_text(wifiLabel_, text);
}

void SettingsScreen::selectTempUnit(TempUnit unit) {
  if (unit == unit_) return;  // already selected
  if (onTempUnitSelected_) onTempUnitSelected_(context_, unit);
}

void SettingsScreen::close() {
  // We're inside the close button's own click event, so the page can't be
  // deleted right now. delete_async removes it on the next LVGL cycle.
  lv_obj_delete_async(overlay_);
  overlay_ = nullptr;
  fahrenheitButton_ = nullptr;
  celsiusButton_ = nullptr;
  wifiLabel_ = nullptr;
}
