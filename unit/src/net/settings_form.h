#pragma once
#include <WString.h>
#include <WebServer.h>

#include "net/page_login.h"
#include "storage/settings_store.h"

// The settings form, shared by the two places a unit is configured from:
// setup mode on its own network (net/provisioning.h) and its page on the home
// network (net/web_pages.h). One form, so the two can't drift apart.
//
// Saved passwords never go into the page, so the password boxes start empty
// and an empty box keeps the saved password — but only for the network or
// broker login it was saved with. Otherwise changing just the broker's
// address would hand the saved login to whatever answers there.
//
// No external files: a phone in setup mode has no internet access, so
// anything it couldn't fetch from the controller wouldn't load at all.
namespace settings_form {

// The start of a page, up to and including a heading that repeats the title.
// `style` is added to the shared stylesheet. End the page with PAGE_END.
String pageStart(const char* title, const char* style = "");
constexpr const char* PAGE_END = "</body></html>";

// Which page the form is on, for the page password's boxes. Setup mode sets
// a new one without asking for the old, which is how a forgotten one is
// replaced; the settings page asks for the current one before changing it.
enum class PasswordBoxes { Setup, Change };

// The form itself, filled in from `settings`. It posts to /save. `error`,
// if there is one, says why the last save was turned down.
String form(const UnitSettings& settings, const PageLogin& login, PasswordBoxes boxes,
            const String& error = "");

// Copies a posted form over `settings`. Anything not sent is left as it was,
// except the passwords, as above.
void read(WebServer& server, UnitSettings& settings);

// The page password's boxes, which aren't saved with the rest: a new password
// is made by PageLogin. Returns an empty string if what was posted can be
// used, with the new password in `password` (empty to keep the one there
// is), or else what's wrong, for form() to show.
String readPagePassword(WebServer& server, PageLogin& login, PasswordBoxes boxes,
                        unsigned long nowMs, String& password);

// The whole page sent just before the restart that applies the settings.
// `message` is HTML, so anything typed into the form must be escaped first.
String saved(const String& message);

// For values going into the page: a network name, say, can contain quotes or
// angle brackets.
String escapeHtml(const char* text);

}  // namespace settings_form
