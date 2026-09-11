#include "ui/ui.h"

// ---------------------------------------------------------------------------
// Styling helpers
// ---------------------------------------------------------------------------

static lv_color_t modeColor(HvacMode mode) {
  switch (mode) {
    case HvacMode::Heat:    return lv_palette_main(LV_PALETTE_DEEP_ORANGE);
    case HvacMode::Cool:    return lv_palette_main(LV_PALETTE_BLUE);
    case HvacMode::Auto:    return lv_palette_main(LV_PALETTE_GREEN);
    case HvacMode::Dry:     return lv_palette_main(LV_PALETTE_TEAL);
    case HvacMode::FanOnly: return lv_palette_main(LV_PALETTE_BLUE_GREY);
    case HvacMode::Off:     break;
  }
  return lv_palette_darken(LV_PALETTE_GREY, 3);
}

static lv_obj_t* makeCaption(lv_obj_t* parent, const char* text, int32_t y) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(label, lv_palette_main(LV_PALETTE_GREY), 0);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
  return label;
}

// A flat button with a label inside. The label is returned via `label`.
//
// Styles are set by hand instead of using LVGL's default theme: its shadows,
// grow-on-press and animated transitions made presses feel laggy on this
// chip (see step 1 in the README).
static lv_obj_t* makeButton(lv_obj_t* parent, int32_t w, int32_t h, lv_obj_t** label) {
  lv_obj_t* button = lv_button_create(parent);
  lv_obj_remove_style_all(button);
  lv_obj_set_size(button, w, h);
  lv_obj_set_style_radius(button, 10, 0);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(button, lv_palette_darken(LV_PALETTE_GREY, 3), 0);

  // Pressed: darken whatever color the button currently is.
  lv_obj_set_style_color_filter_dsc(button, &lv_color_filter_shade, LV_STATE_PRESSED);
  lv_obj_set_style_color_filter_opa(button, LV_OPA_30, LV_STATE_PRESSED);

  // Disabled: fade out. LVGL also ignores taps on disabled objects.
  lv_obj_set_style_bg_opa(button, LV_OPA_40, LV_STATE_DISABLED);
  lv_obj_set_style_text_opa(button, LV_OPA_40, LV_STATE_DISABLED);

  *label = lv_label_create(button);
  lv_obj_set_style_text_font(*label, &lv_font_montserrat_20, 0);
  lv_obj_center(*label);
  return button;
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

void Ui::begin(const char* name, TempUnit unit, const Callbacks& callbacks) {
  unit_ = unit;
  callbacks_ = callbacks;

  lv_obj_t* scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  lv_obj_set_style_text_color(scr, lv_color_white(), 0);

  // Positions below are y offsets from the top of the 320px-tall screen.

  makeCaption(scr, name, 6);

  // Current temperature.
  currentLabel_ = lv_label_create(scr);
  lv_obj_set_style_text_font(currentLabel_, &lv_font_montserrat_48, 0);
  lv_obj_align(currentLabel_, LV_ALIGN_TOP_MID, 0, 24);
  makeCaption(scr, "Indoor", 78);

  // Setpoint with -/+ on either side.
  makeCaption(scr, "Target", 110);
  targetLabel_ = lv_label_create(scr);
  lv_obj_set_style_text_font(targetLabel_, &lv_font_montserrat_28, 0);
  lv_obj_align(targetLabel_, LV_ALIGN_TOP_MID, 0, 126);

  lv_obj_t* label;
  minusButton_ = makeButton(scr, 44, 48, &label);
  lv_label_set_text(label, LV_SYMBOL_MINUS);
  lv_obj_align(minusButton_, LV_ALIGN_TOP_LEFT, 6, 110);
  lv_obj_add_event_cb(
      minusButton_, [](lv_event_t* e) { static_cast<Ui*>(lv_event_get_user_data(e))->onStep(-1); },
      LV_EVENT_CLICKED, this);

  plusButton_ = makeButton(scr, 44, 48, &label);
  lv_label_set_text(label, LV_SYMBOL_PLUS);
  lv_obj_align(plusButton_, LV_ALIGN_TOP_RIGHT, -6, 110);
  lv_obj_add_event_cb(
      plusButton_, [](lv_event_t* e) { static_cast<Ui*>(lv_event_get_user_data(e))->onStep(+1); },
      LV_EVENT_CLICKED, this);

  // Mode: tap to cycle. The button takes on the mode's color.
  modeButton_ = makeButton(scr, 156, 52, &modeLabel_);
  lv_obj_align(modeButton_, LV_ALIGN_TOP_MID, 0, 180);
  lv_obj_add_event_cb(
      modeButton_, [](lv_event_t* e) { static_cast<Ui*>(lv_event_get_user_data(e))->onModeTap(); },
      LV_EVENT_CLICKED, this);

  // Fan speed: tap to cycle.
  lv_obj_t* fanButton = makeButton(scr, 156, 52, &fanLabel_);
  lv_obj_align(fanButton, LV_ALIGN_TOP_MID, 0, 246);
  lv_obj_add_event_cb(
      fanButton, [](lv_event_t* e) { static_cast<Ui*>(lv_event_get_user_data(e))->onFanTap(); },
      LV_EVENT_CLICKED, this);

  show(shown_);  // placeholder "--" values until main.cpp calls show()
}

void Ui::show(const ClimateState& state) {
  shown_ = state;

  char buf[16];
  formatTemp(buf, sizeof(buf), state.currentTempC, unit_);
  lv_label_set_text(currentLabel_, buf);

  formatTemp(buf, sizeof(buf), state.targetTempC, unit_);
  lv_label_set_text(targetLabel_, buf);

  // The setpoint doesn't apply in Off or Fan mode: fade it and lock -/+.
  const bool adjustable = usesTargetTemp(state.mode);
  lv_obj_set_style_text_opa(targetLabel_, adjustable ? LV_OPA_COVER : LV_OPA_40, 0);
  setEnabled(minusButton_, adjustable);
  setEnabled(plusButton_, adjustable);

  lv_label_set_text_fmt(modeLabel_, "Mode: %s", modeName(state.mode));
  lv_obj_set_style_bg_color(modeButton_, modeColor(state.mode), 0);

  lv_label_set_text_fmt(fanLabel_, "Fan: %s", fanSpeedName(state.fan));
}

void Ui::onStep(int direction) {
  if (std::isnan(shown_.targetTempC)) return;

  const float next = stepTargetTemp(shown_.targetTempC, direction, unit_);
  if (next != shown_.targetTempC) callbacks_.setTargetTemp(next);
}

void Ui::onModeTap() { callbacks_.setMode(nextMode(shown_.mode)); }

void Ui::onFanTap() { callbacks_.setFanSpeed(nextFanSpeed(shown_.fan)); }
