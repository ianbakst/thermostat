#pragma once
#include <lvgl.h>

// Small helpers shared by the screens.

// Neutral background for buttons that aren't colored by state.
lv_color_t buttonColor();

// A flat, rounded button with no label.
//
// Styles are set by hand instead of using LVGL's default theme: its shadows,
// grow-on-press and animated transitions made presses feel laggy on this
// chip (see step 1 in the README). Pressed darkens the button; disabled fades
// it and LVGL ignores taps on it.
lv_obj_t* createFlatButton(lv_obj_t* parent, int32_t w, int32_t h);

// A full-screen, opaque black panel on top of everything else on the active
// screen, for pages like the pickers and settings. It swallows taps that
// miss its contents, so nothing underneath can be pressed. Delete it (with
// lv_obj_delete_async from inside one of its own events) to close the page.
lv_obj_t* createOverlay();

// A label that shows an ICON_* in one of the icon fonts (&mdi_icons_24/32/40,
// from ui/icons/icons.h).
lv_obj_t* createIcon(lv_obj_t* parent, const char* icon, const lv_font_t* font);
