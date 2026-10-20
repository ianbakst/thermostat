#include "ui/setup_screen.h"

#include <cstring>

#include "ui/fonts/fonts.h"
#include "ui/widgets.h"

// A line of small text, centered, wrapping if it has to.
static lv_obj_t* makeLine(lv_obj_t* parent, const char* text, int32_t y, lv_color_t color) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(label, 156);
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
  return label;
}

void SetupScreen::open(const char* qrData, const char* line1, const char* line2, const char* hint,
                       bool cancellable, OnCancel onCancel, void* context) {
  if (!showOverlay()) return;  // already open
  onCancel_ = onCancel;
  context_ = context;

  // Only offer a way out if the panel has a network to go back to. The handler
  // is named so both branches of the choice below have the same type.
  const lv_event_cb_t onCancelTap = [](lv_event_t* e) {
    auto* screen = static_cast<SetupScreen*>(lv_event_get_user_data(e));
    if (screen->onCancel_) screen->onCancel_(screen->context_);
  };
  addHeader("Setup", cancellable ? onCancelTap : nullptr, this);

  // The QR code needs a white margin around it ("quiet zone") to scan
  // reliably, so it sits on a white panel a little larger than itself.
  lv_obj_t* frame = lv_obj_create(overlay_);
  lv_obj_remove_style_all(frame);
  lv_obj_set_size(frame, 140, 140);
  lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(frame, lv_color_white(), 0);
  lv_obj_set_style_radius(frame, 4, 0);
  lv_obj_align(frame, LV_ALIGN_TOP_MID, 0, 52);

  lv_obj_t* qr = lv_qrcode_create(frame);
  lv_qrcode_set_size(qr, 124);
  lv_qrcode_set_dark_color(qr, lv_color_black());
  lv_qrcode_set_light_color(qr, lv_color_white());
  lv_qrcode_update(qr, qrData, strlen(qrData));
  lv_obj_center(qr);

  makeLine(overlay_, line1, 198, lv_color_white());
  makeLine(overlay_, line2, 220, lv_color_white());
  makeLine(overlay_, hint, 246, lv_palette_main(LV_PALETTE_GREY));

  progressLabel_ = makeLine(overlay_, "", 292, lv_palette_main(LV_PALETTE_BLUE));
}

void SetupScreen::showProgress(const char* text) {
  if (!progressLabel_) return;
  lv_label_set_text(progressLabel_, text);
}
