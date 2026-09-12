#include "ui/list_screen.h"

#include <cstdint>

#include "ui/fonts/fonts.h"
#include "ui/widgets.h"

void ListScreen::open(const char* title, const char* const* labels, int count, int selected,
                      const char* emptyMessage, OnSelect onSelect, void* context) {
  if (!showOverlay()) return;  // already open
  onSelect_ = onSelect;
  context_ = context;

  addHeader(
      title,
      [](lv_event_t* e) { static_cast<ListScreen*>(lv_event_get_user_data(e))->hideOverlay(); },
      this);

  if (count == 0) {
    lv_obj_t* empty = lv_label_create(overlay_);
    lv_label_set_text(empty, emptyMessage);
    lv_obj_set_style_text_color(empty, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_width(empty, 148);
    lv_label_set_long_mode(empty, LV_LABEL_LONG_WRAP);
    lv_obj_align(empty, LV_ALIGN_TOP_LEFT, 12, 70);
    return;
  }

  // A scrollable column, so any number of choices fits.
  lv_obj_t* list = lv_obj_create(overlay_);
  lv_obj_remove_style_all(list);
  lv_obj_set_size(list, 164, 256);
  lv_obj_set_pos(list, 4, 56);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, 6, 0);
  lv_obj_set_style_pad_all(list, 4, 0);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_style_bg_opa(list, LV_OPA_30, LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_color(list, lv_color_white(), LV_PART_SCROLLBAR);
  lv_obj_set_style_width(list, 4, LV_PART_SCROLLBAR);
  lv_obj_set_style_radius(list, 2, LV_PART_SCROLLBAR);

  for (int i = 0; i < count; i++) {
    lv_obj_t* row = createFlatButton(list, 148, 40);
    if (i == selected) lv_obj_set_style_bg_color(row, lv_palette_main(LV_PALETTE_BLUE), 0);

    lv_obj_t* label = lv_label_create(row);
    lv_label_set_text(label, labels[i]);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_center(label);

    lv_obj_set_user_data(row, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
    lv_obj_add_event_cb(
        row,
        [](lv_event_t* e) {
          lv_obj_t* row = lv_event_get_current_target_obj(e);
          const int index = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(row)));
          static_cast<ListScreen*>(lv_event_get_user_data(e))->onRowClicked(index);
        },
        LV_EVENT_CLICKED, this);

    // Scroll the current choice into view, so a long list opens where it left
    // off rather than at the top.
    if (i == selected) lv_obj_scroll_to_view(row, LV_ANIM_OFF);
  }
}

void ListScreen::onRowClicked(int index) {
  hideOverlay();
  if (onSelect_) onSelect_(context_, index);
}
