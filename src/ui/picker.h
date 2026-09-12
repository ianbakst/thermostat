#pragma once
#include <lvgl.h>

#include "ui/overlay_screen.h"

struct PickerOption {
  const char* icon;   // ICON_* from ui/fonts/fonts.h
  const char* label;
  lv_color_t color;   // icon color, and the tile color when selected
};

// A full-screen grid of choices (up to 6, in 2 columns). Tapping a choice
// closes the picker and reports its index. If nothing is chosen within
// PICKER_TIMEOUT_MS, it closes by itself and reports nothing.
class Picker : public OverlayScreen {
 public:
  // Called with the index of the chosen option. `context` is passed through
  // unchanged (typically the object that opened the picker).
  using OnSelect = void (*)(void* context, int index);

  void open(const char* title, const PickerOption* options, int count, int selected,
            OnSelect onSelect, void* context);

 private:
  void onTileClicked(int index);
  void close();

  lv_timer_t* timeout_ = nullptr;
  OnSelect onSelect_ = nullptr;
  void* context_ = nullptr;
};
