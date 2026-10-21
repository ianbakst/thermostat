#pragma once
#include <DNSServer.h>
#include <WebServer.h>

#include "net/page_login.h"
#include "storage/settings_store.h"

// Setup mode: the controller runs its own Wi-Fi network with one page on it,
// which is how it's told about the home it's been installed in. Nothing is
// compiled into the firmware, so this is the only way it learns anything.
//
// A controller with no saved network starts here. One that has settings only
// comes back when its BOOT button is held (input/setup_button.h), because a
// unit that fell off the Wi-Fi should keep running the heat pump, not drop
// everything to serve a form. Once it's on the network, its own pages there
// (net/web_pages.h) change the same settings without any of this.
//
// The network is open: no password. Getting here takes a blank board or the
// BOOT button, which is inside the mini-split behind a screwed-on cover, so
// whoever can start setup mode already has the unit open. A password would
// only have to be read off the USB log, which an installed unit isn't plugged
// into. For the same reason, this is where the settings password is chosen,
// and where a forgotten one is replaced without the old one (see
// net/page_login.h).
//
// The panel does the same thing with a three-step wizard, testing each step
// as it goes. This one is a single page: a controller has no screen to show
// progress on, and the installer is usually at a laptop with the serial log
// open. What the two have in common — the access point, the captive-portal
// DNS, the page styling — is the next thing worth moving into ../shared.
class Provisioning {
 public:
  // For the BOOT button: start in setup mode after the next restart, which a
  // unit with a saved network otherwise wouldn't. Lost in a power cut.
  static void requestAtNextBoot();

  // Whether setup mode was asked for before this boot. Clears the request,
  // so it applies to one boot only: pressing RESET in setup mode goes back to
  // the saved settings, unchanged.
  static bool takeRequest();

  // Brings up the access point. `settings` fills the form in, so re-running
  // setup doesn't mean retyping everything, and `store` and `login` are what
  // the form writes to.
  void begin(SettingsStore& store, const UnitSettings& settings, PageLogin& login);

  // Call from loop(). Serves the page and answers DNS.
  void tick();

  bool active() const { return active_; }

 private:
  void handleRoot();
  void handleSave();
  void showForm(const String& error);

  WebServer server_{80};
  DNSServer dns_;
  SettingsStore* store_ = nullptr;
  PageLogin* login_ = nullptr;
  UnitSettings draft_;
  bool active_ = false;
};
