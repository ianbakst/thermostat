#pragma once
#include <WString.h>

#include "storage/settings_store.h"

// The HTML for the setup pages. Pure rendering: everything these need is
// passed in, so the wording can be read and reworked without going near the
// state machine that drives them (see net/provisioning.h for the flow).
//
// One plain page per step, no external files: a phone in setup mode has no
// internet access, so anything it couldn't fetch from the panel wouldn't load
// at all.
namespace setup_pages {

struct Context {
  const char* stepLabel;          // "Step 2 of 3"
  const DeviceSettings& draft;    // pre-fills the forms; passwords are never sent
  const char* error = nullptr;    // shown above the form when the last test failed
  const String* networks = nullptr;  // <option> tags for the network dropdown
  const char* apSsid = "";       // the panel's own network, for the "rejoin" hint
  bool canGoBack = false;        // whether there's an earlier step to offer
};

String wifiForm(const Context& context);
String wifiTesting(const Context& context);
String mqttForm(const Context& context);
String mqttTesting(const Context& context);
String unitChoice(const Context& context, const char* const* units, int count);

// `apWillDisappear`: true when the panel was running its own network, which is
// about to go away with the restart.
String done(const Context& context, bool apWillDisappear);

}  // namespace setup_pages
