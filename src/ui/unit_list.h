#pragma once
#include <lvgl.h>

#include "ui/overlay_screen.h"

// A full-screen, scrollable list of the mini-splits Home Assistant advertises,
// for picking the one this panel controls. The current one is highlighted.
// Tapping one reports it and closes the list; the X closes it unchanged.
class UnitList : public OverlayScreen {
 public:
  using OnSelect = void (*)(void* context, const char* name);

  // `names` must stay valid while the list is open (MiniSplit only ever adds
  // to it, so the indexes stay put).
  void open(const char* const* names, int count, const char* selected, OnSelect onSelect,
            void* context);

 private:
  void onRowClicked(int index);

  const char* const* names_ = nullptr;
  int count_ = 0;
  OnSelect onSelect_ = nullptr;
  void* context_ = nullptr;
};
