#pragma once
#include <lvgl.h>

#include "ui/overlay_screen.h"

// A full-screen, scrollable list of labelled choices, with the current one
// highlighted. Tapping one reports its index and closes the list; the X closes
// it unchanged.
//
// Used wherever there are more options than the 6 a Picker's grid holds, or
// where the options are words rather than icons: the mini-splits to control,
// and the two louvre positions.
class ListScreen : public OverlayScreen {
 public:
  using OnSelect = void (*)(void* context, int index);

  // `labels` must stay valid while the list is open. `emptyMessage` is shown
  // instead of the list when there's nothing to choose from.
  void open(const char* title, const char* const* labels, int count, int selected,
            const char* emptyMessage, OnSelect onSelect, void* context);

 private:
  void onRowClicked(int index);

  OnSelect onSelect_ = nullptr;
  void* context_ = nullptr;
};
