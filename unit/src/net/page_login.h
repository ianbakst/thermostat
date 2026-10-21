#pragma once
#include <Preferences.h>
#include <WString.h>

#include <cstdint>

// The password for the unit's settings, and the logins it hands browsers.
//
// The controls are open to anyone on the network, by choice: a visitor can
// turn the heat up. Everything else on the unit's pages — the settings, which
// hold the Wi-Fi and broker logins, and firmware updates — needs this
// password. It's chosen in setup mode, which only the BOOT button gets to,
// and that's also where a forgotten one is replaced.
//
// The password itself is never kept, only a salted PBKDF2 hash of it: enough
// to check one typed in, not enough to recover it, even from a copy of the
// flash.
//
// A browser that gets it right is handed a login: a random number and its
// HMAC under a key that never leaves the board, so nobody can make one up.
// The key is saved too, so logins survive a restart, and a new password comes
// with a new key, which logs every browser out at once. There's no clock, so
// a login can't carry a date the unit could check: the browser drops it
// after PAGE_LOGIN_DAYS without a visit, and until then a copy of it works.
//
// The pages are plain HTTP, so the password and the login cross the Wi-Fi
// unencrypted. Someone able to read its traffic could pick them up.
class PageLogin {
 public:
  enum class Check { Right, Wrong, LockedOut };

  // Loads the saved hash and key, if there are any.
  void begin();

  bool hasPassword() const { return hasPassword_; }

  // Replaces the password, and the key with it.
  void setPassword(const char* password);

  // Whether `password` is the saved one. Slow on purpose (see
  // PAGE_PASSWORD_ROUNDS). After PAGE_LOGIN_MAX_FAILURES wrong in a row, it
  // refuses to check anything for PAGE_LOGIN_LOCKOUT_MS.
  Check check(const char* password, unsigned long nowMs);

  // A login for a browser that just got the password right.
  String newLogin() const;

  // Whether `login` is one this unit handed out since the password last
  // changed.
  bool isLogin(const String& login) const;

  // The Set-Cookie header that gives a browser `login`, or takes it back if
  // `login` is empty.
  static String cookieFor(const String& login);

  // The login in a request's Cookie header, or empty if there isn't one.
  static String loginIn(const String& cookies);

 private:
  void hash(const char* password, uint8_t* out) const;
  void sign(const uint8_t* nonce, uint8_t* out) const;

  Preferences prefs_;
  bool hasPassword_ = false;
  uint8_t salt_[16] = {};
  uint8_t hash_[32] = {};
  uint8_t key_[32] = {};

  int failures_ = 0;
  bool locked_ = false;
  unsigned long lockedSinceMs_ = 0;
};
