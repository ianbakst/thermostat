#include "ui/ui.h"

#include <cstring>

#include "ui/fonts/fonts.h"
#include "ui/widgets.h"

// ---------------------------------------------------------------------------
// How each setting looks
// ---------------------------------------------------------------------------

static const char* modeIcon(HvacMode mode) {
  switch (mode) {
    case HvacMode::Off:     return ICON_POWER;
    case HvacMode::Heat:    return ICON_FIRE;
    case HvacMode::Cool:    return ICON_SNOWFLAKE;
    case HvacMode::Auto:    return ICON_SUN_SNOWFLAKE_VARIANT;
    case HvacMode::Dry:     return ICON_WATER_PERCENT;
    case HvacMode::FanOnly: return ICON_FAN;
  }
  return ICON_POWER;
}

static lv_palette_t modePalette(HvacMode mode) {
  switch (mode) {
    case HvacMode::Heat:    return LV_PALETTE_DEEP_ORANGE;
    case HvacMode::Cool:    return LV_PALETTE_BLUE;
    case HvacMode::Auto:    return LV_PALETTE_GREEN;
    case HvacMode::Dry:     return LV_PALETTE_TEAL;
    case HvacMode::FanOnly: return LV_PALETTE_BLUE_GREY;
    case HvacMode::Off:     break;
  }
  return LV_PALETTE_GREY;
}

// Bright version, for icons and highlights in the mode picker.
static lv_color_t modeColor(HvacMode mode) { return lv_palette_main(modePalette(mode)); }

// Darker version for the whole screen's background, so white text stays
// readable. Off is plain black.
static lv_color_t modeBackground(HvacMode mode) {
  if (mode == HvacMode::Off) return lv_color_black();
  return lv_palette_darken(modePalette(mode), 3);
}

static const char* fanIcon(FanMode fan) {
  switch (fan) {
    case FanMode::Low:     return ICON_FAN_SPEED_1;
    case FanMode::Medium:  return ICON_FAN_SPEED_2;
    case FanMode::High:    return ICON_FAN_SPEED_3;
    case FanMode::Auto:    return ICON_FAN_AUTO;
    case FanMode::Off:     return ICON_FAN_OFF;
    case FanMode::Ambient: return ICON_WEATHER_WINDY;
  }
  return ICON_FAN_AUTO;
}

static const char* wifiIcon(const WifiStatus& status) {
  if (!status.connected) return ICON_WIFI_STRENGTH_OFF_OUTLINE;
  switch (signalBars(status.rssi)) {
    case 4:  return ICON_WIFI_STRENGTH_4;
    case 3:  return ICON_WIFI_STRENGTH_3;
    case 2:  return ICON_WIFI_STRENGTH_2;
    default: return ICON_WIFI_STRENGTH_1;
  }
}

// A triangle-shaped setpoint control with a black +/- inside. The icon glyph
// is itself the tappable object, so there's no rectangular button behind it.
// Disabled fades it and LVGL ignores taps on it.
//
// `signOffsetY` moves the sign from the middle of the icon's box to the
// middle of the triangle, which sits lower in an up-triangle (wide base at
// the bottom) and higher in a down-triangle. From the 40px glyph metrics:
// up is drawn at y 1-33 with its inner center at ~22, down at y 3-35 with
// its inner center at ~14, in a 37px-tall box (middle at 18.5).
static lv_obj_t* makeStepArrow(lv_obj_t* parent, const char* icon, const char* sign,
                               int32_t signOffsetY) {
  lv_obj_t* arrow = createIcon(parent, icon, &mdi_icons_40);
  lv_obj_add_flag(arrow, LV_OBJ_FLAG_CLICKABLE);  // labels aren't by default
  lv_obj_set_ext_click_area(arrow, 12);
  lv_obj_set_style_text_opa(arrow, LV_OPA_40, LV_STATE_DISABLED);

  lv_obj_t* signLabel = lv_label_create(arrow);
  lv_label_set_text(signLabel, sign);
  lv_obj_set_style_text_font(signLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(signLabel, lv_color_black(), 0);
  lv_obj_align(signLabel, LV_ALIGN_CENTER, 0, signOffsetY);
  return arrow;
}

// The main screen's background changes color with the mode, so its buttons
// can't have a fixed color that suits every background. Instead they're
// translucent white: a lighter tint of whatever color is behind them.
static void tintButton(lv_obj_t* button) {
  lv_obj_set_style_bg_color(button, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(button, LV_OPA_20, 0);
}

static void setEnabled(lv_obj_t* obj, bool enabled) {
  if (enabled) {
    lv_obj_remove_state(obj, LV_STATE_DISABLED);
  } else {
    lv_obj_add_state(obj, LV_STATE_DISABLED);
  }
}

// ---------------------------------------------------------------------------
// Ui
// ---------------------------------------------------------------------------

void Ui::begin(TempUnit unit, const Callbacks& callbacks) {
  unit_ = unit;
  callbacks_ = callbacks;

  lv_obj_t* scr = lv_screen_active();
  screen_ = scr;
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);  // show() sets it per mode
  lv_obj_set_style_text_color(scr, lv_color_white(), 0);

  // Positions below are offsets on the 172x320 screen.

  // Top-left: settings. Just the gear, no button behind it: the icon itself
  // is tappable, with a 16px margin so the tap target stays ~56px square.
  lv_obj_t* settingsButton = createIcon(scr, ICON_COG, &mdi_icons_24);
  lv_obj_add_flag(settingsButton, LV_OBJ_FLAG_CLICKABLE);  // labels aren't by default
  lv_obj_set_ext_click_area(settingsButton, 16);
  lv_obj_align(settingsButton, LV_ALIGN_TOP_LEFT, 16, 17);
  lv_obj_add_event_cb(
      settingsButton, [](lv_event_t* e) { static_cast<Ui*>(lv_event_get_user_data(e))->openSettings(); },
      LV_EVENT_CLICKED, this);

  // Top-right corner: Wi-Fi status icon, like a phone's.
  wifiIcon_ = createIcon(scr, ICON_WIFI_STRENGTH_OFF_OUTLINE, &mdi_icons_24);
  lv_obj_align(wifiIcon_, LV_ALIGN_TOP_RIGHT, -8, 17);

  // Top center: current temperature with its unit. Even a wide value like
  // "21.5°C" (~62px) stays clear of the settings button and Wi-Fi icon.
  currentLabel_ = lv_label_create(scr);
  lv_obj_set_style_text_font(currentLabel_, &lv_font_montserrat_20, 0);
  lv_obj_align(currentLabel_, LV_ALIGN_TOP_MID, 0, 17);

  // Center: target temperature, with up/down arrows above and below it
  // (24px clear of the number). temperature_48 only contains the characters
  // a temperature needs, so its line box is shorter than a full font's; the
  // digits are drawn at y 135-169.
  targetLabel_ = lv_label_create(scr);
  lv_obj_set_style_text_font(targetLabel_, &temperature_48, 0);
  lv_obj_align(targetLabel_, LV_ALIGN_TOP_MID, 0, 134);

  upButton_ = makeStepArrow(scr, ICON_TRIANGLE, LV_SYMBOL_PLUS, 4);
  lv_obj_align(upButton_, LV_ALIGN_TOP_MID, 0, 65);
  lv_obj_add_event_cb(
      upButton_, [](lv_event_t* e) { static_cast<Ui*>(lv_event_get_user_data(e))->onStep(+1); },
      LV_EVENT_CLICKED, this);

  downButton_ = makeStepArrow(scr, ICON_TRIANGLE_DOWN, LV_SYMBOL_MINUS, -5);
  lv_obj_align(downButton_, LV_ALIGN_TOP_MID, 0, 202);
  lv_obj_add_event_cb(
      downButton_, [](lv_event_t* e) { static_cast<Ui*>(lv_event_get_user_data(e))->onStep(-1); },
      LV_EVENT_CLICKED, this);

  // Bottom corners: mode and fan. The screen is ~0.1mm per pixel, so these
  // small buttons also accept taps a few pixels outside their edges.
  const int32_t cornerSize = 56;
  const int32_t cornerMargin = 8;
  const int32_t cornerTapSlop = 8;

  // Bottom-left: mode.
  lv_obj_t* modeButton = createFlatButton(scr, cornerSize, cornerSize);
  tintButton(modeButton);
  lv_obj_align(modeButton, LV_ALIGN_BOTTOM_LEFT, cornerMargin, -cornerMargin);
  lv_obj_set_ext_click_area(modeButton, cornerTapSlop);
  modeIcon_ = createIcon(modeButton, ICON_POWER, &mdi_icons_32);
  lv_obj_center(modeIcon_);
  lv_obj_add_event_cb(
      modeButton, [](lv_event_t* e) { static_cast<Ui*>(lv_event_get_user_data(e))->openModePicker(); },
      LV_EVENT_CLICKED, this);

  // Bottom-right: fan.
  lv_obj_t* fanButton = createFlatButton(scr, cornerSize, cornerSize);
  tintButton(fanButton);
  lv_obj_align(fanButton, LV_ALIGN_BOTTOM_RIGHT, -cornerMargin, -cornerMargin);
  lv_obj_set_ext_click_area(fanButton, cornerTapSlop);
  fanIcon_ = createIcon(fanButton, ICON_FAN_AUTO, &mdi_icons_32);
  lv_obj_center(fanIcon_);
  lv_obj_add_event_cb(
      fanButton, [](lv_event_t* e) { static_cast<Ui*>(lv_event_get_user_data(e))->openFanPicker(); },
      LV_EVENT_CLICKED, this);

  show(shown_);  // placeholder "--" values until main.cpp calls show()
}

void Ui::show(const ClimateState& state) {
  shown_ = state;

  char buf[16];
  formatTemp(buf, sizeof(buf), state.currentTempC, unit_, true /* showUnit */);
  lv_label_set_text(currentLabel_, buf);

  formatTemp(buf, sizeof(buf), state.targetTempC, unit_, true /* showUnit */);
  lv_label_set_text(targetLabel_, buf);

  // The setpoint doesn't apply in Off or Fan mode: fade it and lock the arrows.
  const bool adjustable = usesTargetTemp(state.mode);
  lv_obj_set_style_text_opa(targetLabel_, adjustable ? LV_OPA_COVER : LV_OPA_40, 0);
  setEnabled(upButton_, adjustable);
  setEnabled(downButton_, adjustable);

  // The whole background shows the mode; the mode button's icon names it.
  lv_obj_set_style_bg_color(screen_, modeBackground(state.mode), 0);
  lv_label_set_text(modeIcon_, modeIcon(state.mode));

  lv_label_set_text(fanIcon_, fanIcon(state.fan));
}

void Ui::onStep(int direction) {
  if (std::isnan(shown_.targetTempC)) return;

  const float next = stepTargetTemp(shown_.targetTempC, direction, unit_);
  if (next != shown_.targetTempC) callbacks_.setTargetTemp(next);
}

void Ui::showTempUnit(TempUnit unit) {
  unit_ = unit;
  show(shown_);
  settingsScreen_.showTempUnit(unit);
}

void Ui::showWifiStatus(const WifiStatus& status) {
  // Called every few seconds; only redraw the icon when it actually changes.
  const char* icon = wifiIcon(status);
  if (strcmp(lv_label_get_text(wifiIcon_), icon) != 0) lv_label_set_text(wifiIcon_, icon);

  settingsScreen_.showWifiStatus(status);
}

void Ui::openSettings() {
  settingsScreen_.open(
      unit_,
      [](void* context, TempUnit unit) { static_cast<Ui*>(context)->callbacks_.setTempUnit(unit); },
      this);
}

void Ui::openModePicker() {
  PickerOption options[HVAC_MODE_COUNT];
  for (int i = 0; i < HVAC_MODE_COUNT; i++) {
    const auto mode = static_cast<HvacMode>(i);
    options[i] = {modeIcon(mode), modeName(mode), modeColor(mode)};
  }
  picker_.open(
      "Mode", options, HVAC_MODE_COUNT, static_cast<int>(shown_.mode),
      [](void* context, int index) {
        static_cast<Ui*>(context)->callbacks_.setMode(static_cast<HvacMode>(index));
      },
      this);
}

void Ui::openFanPicker() {
  PickerOption options[FAN_MODE_COUNT];
  for (int i = 0; i < FAN_MODE_COUNT; i++) {
    const auto fan = static_cast<FanMode>(i);
    options[i] = {fanIcon(fan), fanModeName(fan), lv_palette_main(LV_PALETTE_BLUE)};
  }
  picker_.open(
      "Fan", options, FAN_MODE_COUNT, static_cast<int>(shown_.fan),
      [](void* context, int index) {
        static_cast<Ui*>(context)->callbacks_.setFanMode(static_cast<FanMode>(index));
      },
      this);
}
