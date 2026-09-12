#pragma once
#include <lvgl.h>

#include "ui/overlay_screen.h"

// The setup screen (portrait, 172x320), shown while the panel is serving its
// setup pages (see src/net/provisioning.h). Either it's running its own Wi-Fi
// network to join:
//
//   [X] Setup             X only when there's a network to go back to
//
//    +-----------+
//    |  QR code  |        scan to join the panel's network...
//    +-----------+
//
//   Thermostat-39FEFF     ...or join it by hand: the name,
//   Password: kx7mqrst    and a password made for this session
//
//   Scan to join, then
//   the page opens        what to do
//
//   Step 1 of 3: Wi-Fi    how far the setup has got
//
// or, when only the broker is being changed, the same layout with the panel's
// address on your own network instead (the QR code opens it directly).
//
// The setup pages themselves are served by the panel, not drawn here. There's
// no close(): setup always ends in a restart, which clears the screen anyway.
class SetupScreen : public OverlayScreen {
 public:
  using OnCancel = void (*)(void* context);

  // `qrData` is what the QR code carries: a "join this network" code, or the
  // address of the pages. The two lines and the hint below it say the same
  // thing in words, for anyone typing it in.
  void open(const char* qrData, const char* line1, const char* line2, const char* hint,
            bool cancellable, OnCancel onCancel, void* context);

  // Which step the person on the other end is on.
  void showProgress(const char* text);

 private:
  lv_obj_t* progressLabel_ = nullptr;
  OnCancel onCancel_ = nullptr;
  void* context_ = nullptr;
};
