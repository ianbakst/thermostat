#include "ui/widgets.h"

lv_color_t buttonColor() { return lv_palette_darken(LV_PALETTE_GREY, 3); }

lv_obj_t* createFlatButton(lv_obj_t* parent, int32_t w, int32_t h) {
  lv_obj_t* button = lv_button_create(parent);
  lv_obj_remove_style_all(button);
  lv_obj_set_size(button, w, h);
  lv_obj_set_style_radius(button, 10, 0);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(button, buttonColor(), 0);

  // Pressed: darken whatever color the button currently is.
  lv_obj_set_style_color_filter_dsc(button, &lv_color_filter_shade, LV_STATE_PRESSED);
  lv_obj_set_style_color_filter_opa(button, LV_OPA_30, LV_STATE_PRESSED);

  // Disabled: fade out.
  lv_obj_set_style_bg_opa(button, LV_OPA_40, LV_STATE_DISABLED);
  lv_obj_set_style_text_opa(button, LV_OPA_40, LV_STATE_DISABLED);
  return button;
}

lv_obj_t* createOverlay() {
  lv_obj_t* overlay = lv_obj_create(lv_screen_active());
  lv_obj_remove_style_all(overlay);
  lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
  lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
  return overlay;
}

lv_obj_t* createIcon(lv_obj_t* parent, const char* icon, const lv_font_t* font) {
  lv_obj_t* label = lv_label_create(parent);
  lv_obj_set_style_text_font(label, font, 0);
  lv_label_set_text(label, icon);
  return label;
}
