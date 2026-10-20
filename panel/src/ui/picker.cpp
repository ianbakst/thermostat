#include "ui/picker.h"

#include <cstdint>

#include "config.h"
#include "ui/fonts/fonts.h"
#include "ui/widgets.h"

// Layout for the 172x320 screen: title, then a 2-column grid of tiles.
static const int32_t TILE_W = 76;
static const int32_t TILE_H = 84;
static const int32_t GAP = 8;
static const int32_t MARGIN_X = 6;
static const int32_t GRID_TOP = 44;

void Picker::open(const char* title, const PickerOption* options, int count, int selected,
                  OnSelect onSelect, void* context) {
  if (!showOverlay()) return;  // already open
  onSelect_ = onSelect;
  context_ = context;

  // Centered title and no X: a picker is dismissed by choosing, or by waiting.
  lv_obj_t* heading = lv_label_create(overlay_);
  lv_label_set_text(heading, title);
  lv_obj_set_style_text_font(heading, &lv_font_montserrat_20, 0);
  lv_obj_align(heading, LV_ALIGN_TOP_MID, 0, 10);

  for (int i = 0; i < count && i < 6; i++) {
    const PickerOption& option = options[i];
    const bool isSelected = i == selected;

    lv_obj_t* tile = createFlatButton(overlay_, TILE_W, TILE_H);
    lv_obj_set_pos(tile, MARGIN_X + (i % 2) * (TILE_W + GAP), GRID_TOP + (i / 2) * (TILE_H + GAP));
    if (isSelected) lv_obj_set_style_bg_color(tile, option.color, 0);

    // The selected tile is filled with the option's color, so its icon is
    // white; the others show the color on the icon instead.
    lv_obj_t* icon = createIcon(tile, option.icon, &mdi_icons_40);
    lv_obj_set_style_text_color(icon, isSelected ? lv_color_white() : option.color, 0);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t* label = lv_label_create(tile);
    lv_label_set_text(label, option.label);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -10);

    // Remember which option this tile is, for the click handler.
    lv_obj_set_user_data(tile, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
    lv_obj_add_event_cb(
        tile,
        [](lv_event_t* e) {
          lv_obj_t* tile = lv_event_get_current_target_obj(e);
          const int index = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(tile)));
          static_cast<Picker*>(lv_event_get_user_data(e))->onTileClicked(index);
        },
        LV_EVENT_CLICKED, this);
  }

  // If nothing is chosen in time, close without changing anything. LVGL
  // timers run from lv_timer_handler(), on the same task as the UI.
  timeout_ = lv_timer_create(
      [](lv_timer_t* timer) { static_cast<Picker*>(lv_timer_get_user_data(timer))->close(); },
      PICKER_TIMEOUT_MS, this);
}

void Picker::onTileClicked(int index) {
  close();
  if (onSelect_) onSelect_(context_, index);
}

void Picker::close() {
  if (!isOpen()) return;
  hideOverlay();

  // Deleting a timer from inside its own callback is fine in LVGL.
  lv_timer_delete(timeout_);
  timeout_ = nullptr;
}
