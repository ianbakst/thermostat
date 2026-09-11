#pragma once
#include <lvgl.h>

struct PickerOption {
  const char* icon;   // ICON_* from ui/icons/icons.h
  const char* label;
  lv_color_t color;   // icon color, and the tile color when selected
};

// A full-screen grid of choices (up to 6, in 2 columns) that covers the
// current screen. Tapping a choice closes the picker and reports its index.
//
// The picker is built when opened and deleted when closed, so it costs no
// memory or drawing time the rest of the time.
class Picker {
 public:
  // Called with the index of the chosen option. `context` is passed through
  // unchanged (typically the object that opened the picker).
  using OnSelect = void (*)(void* context, int index);

  void open(const char* title, const PickerOption* options, int count, int selected,
            OnSelect onSelect, void* context);

 private:
  void onTileClicked(int index);

  lv_obj_t* overlay_ = nullptr;
  OnSelect onSelect_ = nullptr;
  void* context_ = nullptr;
};
