#include "ui/overlay_screen.h"

#include "config.h"
#include "ui/fonts/fonts.h"
#include "ui/widgets.h"

lv_obj_t* OverlayScreen::showOverlay() {
  if (overlay_) return nullptr;  // already open
  overlay_ = createOverlay();
  return overlay_;
}

void OverlayScreen::hideOverlay() {
  if (!overlay_) return;
  // This often runs inside a click event on one of the overlay's own children,
  // where it can't be deleted yet; delete_async removes it a cycle later.
  lv_obj_delete_async(overlay_);
  overlay_ = nullptr;
}

// The close button: 40x40 at the top-left corner, so its centre is 28 down.
static constexpr int32_t CLOSE_BUTTON_MID_Y = 28;
static constexpr int32_t TITLE_LEFT = 60;      // clear of that button
static constexpr int32_t TITLE_LEFT_NO_CLOSE = 16;

void OverlayScreen::addHeader(const char* title, lv_event_cb_t onClose, void* userData) {
  if (onClose) {
    // Top-left, the same spot as the button that opened the page.
    lv_obj_t* closeButton = createFlatButton(overlay_, 40, 40);
    lv_obj_align(closeButton, LV_ALIGN_TOP_LEFT, 8, 8);
    lv_obj_set_ext_click_area(closeButton, 8);
    lv_obj_center(createIcon(closeButton, ICON_CLOSE, &mdi_icons_24));
    lv_obj_add_event_cb(closeButton, onClose, LV_EVENT_CLICKED, userData);
  }

  const int32_t left = onClose ? TITLE_LEFT : TITLE_LEFT_NO_CLOSE;

  lv_obj_t* label = lv_label_create(overlay_);
  lv_label_set_text(label, title);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);

  // Titles wrap rather than run off the 172px panel: "Update channel" doesn't
  // fit beside the close button on one line. The wrapped block is then centred
  // on the button rather than pinned to a fixed y, so one and two-line titles
  // both sit level with it and a second line grows upwards into the empty
  // space instead of down into the page's contents.
  lv_obj_set_width(label, TFT_PANEL_WIDTH - left - 8);
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  lv_obj_update_layout(label);  // so the height below is the wrapped height
  lv_obj_align(label, LV_ALIGN_TOP_LEFT, left, CLOSE_BUTTON_MID_Y - lv_obj_get_height(label) / 2);
}
