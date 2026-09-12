#include "ui/unit_list.h"

#include <cstdint>
#include <cstring>

#include "ui/fonts/fonts.h"
#include "ui/widgets.h"

void UnitList::open(const char* const* names, int count, const char* selected, OnSelect onSelect,
                    void* context) {
  if (!showOverlay()) return;  // already open
  names_ = names;
  count_ = count;
  onSelect_ = onSelect;
  context_ = context;

  addHeader(
      "Mini-split",
      [](lv_event_t* e) { static_cast<UnitList*>(lv_event_get_user_data(e))->hideOverlay(); }, this);

  if (count_ == 0) {
    lv_obj_t* empty = lv_label_create(overlay_);
    lv_label_set_text(empty,
                      "No mini-splits found yet. They appear once the panel reaches the MQTT "
                      "broker; check the Wi-Fi section for why it hasn't.");
    lv_obj_set_style_text_color(empty, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_width(empty, 148);
    lv_label_set_long_mode(empty, LV_LABEL_LONG_WRAP);
    lv_obj_align(empty, LV_ALIGN_TOP_LEFT, 12, 70);
    return;
  }

  // A scrollable column, so any number of units fits.
  lv_obj_t* list = lv_obj_create(overlay_);
  lv_obj_remove_style_all(list);
  lv_obj_set_size(list, 164, 256);
  lv_obj_set_pos(list, 4, 56);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, 6, 0);
  lv_obj_set_style_pad_all(list, 4, 0);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);

  for (int i = 0; i < count_; i++) {
    lv_obj_t* row = createFlatButton(list, 148, 40);
    if (strcmp(names_[i], selected) == 0) {
      lv_obj_set_style_bg_color(row, lv_palette_main(LV_PALETTE_BLUE), 0);
    }

    lv_obj_t* label = lv_label_create(row);
    lv_label_set_text(label, names_[i]);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_center(label);

    lv_obj_set_user_data(row, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
    lv_obj_add_event_cb(
        row,
        [](lv_event_t* e) {
          lv_obj_t* row = lv_event_get_current_target_obj(e);
          const int index = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(row)));
          static_cast<UnitList*>(lv_event_get_user_data(e))->onRowClicked(index);
        },
        LV_EVENT_CLICKED, this);
  }
}

void UnitList::onRowClicked(int index) {
  const char* name = names_[index];
  hideOverlay();
  if (onSelect_) onSelect_(context_, name);
}
