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
  if (overlay_) return;  // already open
  onCancel_ = onCancel;
  context_ = context;

  overlay_ = createOverlay();

  lv_obj_t* title = lv_label_create(overlay_);
  lv_label_set_text(title, "Setup");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 14);

  // Only offer a way out if the panel has a network to go back to.
  if (cancellable) {
    lv_obj_t* closeButton = createFlatButton(overlay_, 40, 40);
    lv_obj_align(closeButton, LV_ALIGN_TOP_LEFT, 4, 4);
    lv_obj_set_ext_click_area(closeButton, 8);
    lv_obj_center(createIcon(closeButton, ICON_CLOSE, &mdi_icons_24));
    lv_obj_add_event_cb(
        closeButton,
        [](lv_event_t* e) {
          auto* screen = static_cast<SetupScreen*>(lv_event_get_user_data(e));
          if (screen->onCancel_) screen->onCancel_(screen->context_);
        },
        LV_EVENT_CLICKED, this);
  }

  // The QR code needs a white margin around it ("quiet zone") to scan
  // reliably, so it sits on a white panel a little larger than itself.
  lv_obj_t* frame = lv_obj_create(overlay_);
  lv_obj_remove_style_all(frame);
  lv_obj_set_size(frame, 140, 140);
  lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(frame, lv_color_white(), 0);
  lv_obj_set_style_radius(frame, 4, 0);
  lv_obj_align(frame, LV_ALIGN_TOP_MID, 0, 46);

  lv_obj_t* qr = lv_qrcode_create(frame);
  lv_qrcode_set_size(qr, 124);
  lv_qrcode_set_dark_color(qr, lv_color_black());
  lv_qrcode_set_light_color(qr, lv_color_white());
  lv_qrcode_update(qr, qrData, strlen(qrData));
  lv_obj_center(qr);

  makeLine(overlay_, line1, 196, lv_color_white());
  makeLine(overlay_, line2, 218, lv_color_white());
  makeLine(overlay_, hint, 244, lv_palette_main(LV_PALETTE_GREY));

  progressLabel_ = makeLine(overlay_, "", 290, lv_palette_main(LV_PALETTE_BLUE));
}

void SetupScreen::showProgress(const char* text) {
  if (!progressLabel_) return;
  lv_label_set_text(progressLabel_, text);
}

void SetupScreen::close() {
  if (!overlay_) return;
  // This can run inside the close button's own click event, where the screen
  // can't be deleted yet; delete_async removes it on the next LVGL cycle.
  lv_obj_delete_async(overlay_);
  overlay_ = nullptr;
  progressLabel_ = nullptr;
}
