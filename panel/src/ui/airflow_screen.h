#pragma once
#include <lvgl.h>

#include "ui/overlay_screen.h"

// The airflow page (portrait, 172x320), behind the bottom-centre button:
//
//   [X] Airflow
//
//   Vertical
//   [ Swing ]             tapping opens the list of vane positions
//
//   Horizontal
//   [ Center ]            ...and of wide-vane positions
//
// Both louvres in one place, each button showing where it currently points.
// The vertical one is Home Assistant's swing mode; the horizontal one isn't
// exposed to HA at all, so this is the only easy way to set it.
class AirflowScreen : public OverlayScreen {
 public:
  struct Callbacks {
    void (*openVaneList)(void* context);
    void (*openWideVaneList)(void* context);
  };

  void open(const Callbacks& callbacks, void* context, const char* vertical,
            const char* horizontal);

  // Updates both buttons when the unit reports a new position.
  void showPositions(const char* vertical, const char* horizontal);

 private:
  void close();

  lv_obj_t* verticalLabel_ = nullptr;
  lv_obj_t* horizontalLabel_ = nullptr;
  Callbacks callbacks_{};
  void* context_ = nullptr;
};
