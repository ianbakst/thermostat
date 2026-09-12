#pragma once
#include <lvgl.h>

// Shared lifecycle for the full-screen pages that cover the thermostat screen:
// the settings page, the setup screen, the mode/fan pickers and the unit list.
//
// Each is built when opened and deleted when closed, so it costs no memory or
// drawing time the rest of the time, and each sits on an opaque panel that
// swallows taps missing its contents. That part is the same everywhere, so it
// lives here and subclasses only add their own content.
class OverlayScreen {
 public:
  bool isOpen() const { return overlay_ != nullptr; }

 protected:
  // Creates the overlay and returns it, or nullptr if this screen is already
  // open — in which case the caller should leave it alone.
  lv_obj_t* showOverlay();

  // Deletes the overlay on the next LVGL cycle, so this is safe to call from
  // inside an event of one of its own children. Subclasses that cache child
  // pointers have to forget them here too.
  void hideOverlay();

  // The title bar the dismissable pages share: an X in the top-left corner
  // that calls `onClose` with `userData`, and the title beside it. Pass
  // nullptr for a page with no way out (first-run setup).
  void addHeader(const char* title, lv_event_cb_t onClose, void* userData);

  lv_obj_t* overlay_ = nullptr;
};
