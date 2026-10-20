#pragma once
#include <WString.h>
#include <WebServer.h>

#include "storage/settings_store.h"

// The settings form, shared by the two places a unit is configured from:
// setup mode on its own network (net/provisioning.h) and its page on the home
// network (net/web_pages.h). One form, so the two can't drift apart.
//
// Saved passwords never go into the page. On the home network anyone who can
// load it would read them, so the password boxes start empty and an empty box
// keeps the saved password — but only for the network or broker login it was
// saved with. Otherwise changing just the broker's address would hand the
// saved login to whatever answers there.
//
// No external files: a phone in setup mode has no internet access, so
// anything it couldn't fetch from the controller wouldn't load at all.
namespace settings_form {

// The start of a page, up to and including a heading that repeats the title.
// `style` is added to the shared stylesheet. End the page with PAGE_END.
String pageStart(const char* title, const char* style = "");
constexpr const char* PAGE_END = "</body></html>";

// The form itself, filled in from `settings`. It posts to /save.
String form(const UnitSettings& settings);

// Copies a posted form over `settings`. Anything not sent is left as it was,
// except the passwords, as above.
void read(WebServer& server, UnitSettings& settings);

// The whole page sent just before the restart that applies the settings.
// `message` is HTML, so anything typed into the form must be escaped first.
String saved(const String& message);

// For values going into the page: a network name, say, can contain quotes or
// angle brackets.
String escapeHtml(const char* text);

}  // namespace settings_form
