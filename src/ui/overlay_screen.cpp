#include "ui/overlay_screen.h"

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

void OverlayScreen::addHeader(const char* title, lv_event_cb_t onClose, void* userData) {
  if (onClose) {
    // Top-left, the same spot as the button that opened the page.
    lv_obj_t* closeButton = createFlatButton(overlay_, 40, 40);
    lv_obj_align(closeButton, LV_ALIGN_TOP_LEFT, 8, 8);
    lv_obj_set_ext_click_area(closeButton, 8);
    lv_obj_center(createIcon(closeButton, ICON_CLOSE, &mdi_icons_24));
    lv_obj_add_event_cb(closeButton, onClose, LV_EVENT_CLICKED, userData);
  }

  lv_obj_t* label = lv_label_create(overlay_);
  lv_label_set_text(label, title);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
  lv_obj_align(label, LV_ALIGN_TOP_LEFT, onClose ? 60 : 16, 17);
}
