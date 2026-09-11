#pragma once
#include <lvgl.h>

#include "climate/temperature.h"

// The settings page (portrait, 172x320):
//
//   [X] Settings            X closes the page
//
//   Units
//   [  °F  ] [  °C  ]       current choice highlighted
//
// Unlike the pickers, choosing something doesn't close the page; only the X
// does. Like the rest of the UI it doesn't change settings itself: a choice
// calls back to the owner, who applies it and calls showTempUnit().
//
// The page is built when opened and deleted when closed.
class SettingsScreen {
 public:
  using OnTempUnitSelected = void (*)(void* context, TempUnit unit);

  void open(TempUnit unit, OnTempUnitSelected onTempUnitSelected, void* context);

  // Updates the highlighted unit, if the page is open.
  void showTempUnit(TempUnit unit);

 private:
  void selectTempUnit(TempUnit unit);
  void close();

  lv_obj_t* overlay_ = nullptr;
  lv_obj_t* fahrenheitButton_ = nullptr;
  lv_obj_t* celsiusButton_ = nullptr;
  TempUnit unit_ = TempUnit::Fahrenheit;
  OnTempUnitSelected onTempUnitSelected_ = nullptr;
  void* context_ = nullptr;
};
