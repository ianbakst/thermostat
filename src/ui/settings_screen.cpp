#include "ui/settings_screen.h"

#include "ui/icons/icons.h"
#include "ui/widgets.h"

static lv_color_t selectedColor() { return lv_palette_main(LV_PALETTE_BLUE); }

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
  lv_obj_t* caption = lv_label_create(overlay_);
  lv_label_set_text(caption, "Units");
  lv_obj_set_style_text_font(caption, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(caption, lv_palette_main(LV_PALETTE_GREY), 0);
  lv_obj_set_pos(caption, 12, 70);

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

  showTempUnit(unit);
}

void SettingsScreen::showTempUnit(TempUnit unit) {
  unit_ = unit;
  if (!overlay_) return;

  const bool fahrenheit = unit == TempUnit::Fahrenheit;
  lv_obj_set_style_bg_color(fahrenheitButton_, fahrenheit ? selectedColor() : buttonColor(), 0);
  lv_obj_set_style_bg_color(celsiusButton_, fahrenheit ? buttonColor() : selectedColor(), 0);
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
}
