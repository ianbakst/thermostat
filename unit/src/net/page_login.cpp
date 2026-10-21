#include "net/page_login.h"

#include <Arduino.h>
#include <esp_random.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>

#include <cstring>

#include "config.h"

// NVS keys are limited to 15 characters.
static const char* NAMESPACE = "page_login";
static const char* KEY_SALT = "salt";
static const char* KEY_HASH = "hash";
static const char* KEY_SIGNING = "key";

static const char* COOKIE = "login";
static constexpr size_t NONCE_SIZE = 16;
static constexpr size_t MAC_SIZE = 32;  // HMAC-SHA256

// Compares every byte rather than stopping at the first difference, so how
// long it takes says nothing about how close a guess was.
static bool sameBytes(const uint8_t* a, const uint8_t* b, size_t size) {
  uint8_t difference = 0;
  for (size_t i = 0; i < size; i++) difference |= a[i] ^ b[i];
  return difference == 0;
}

static String toHex(const uint8_t* bytes, size_t size) {
  static const char* DIGITS = "0123456789abcdef";
  String out;
  out.reserve(size * 2);
  for (size_t i = 0; i < size; i++) {
    out += DIGITS[bytes[i] >> 4];
    out += DIGITS[bytes[i] & 0x0F];
  }
  return out;
}

// Reads exactly `size` bytes of lowercase hex, as toHex() writes them.
static bool fromHex(const char* text, uint8_t* out, size_t size) {
  for (size_t i = 0; i < size; i++) {
    uint8_t value = 0;
    for (int half = 0; half < 2; half++) {
      const char c = text[i * 2 + half];  // the string's end fails here, not past it
      uint8_t digit;
      if (c >= '0' && c <= '9') {
        digit = c - '0';
      } else if (c >= 'a' && c <= 'f') {
        digit = c - 'a' + 10;
      } else {
        return false;
      }
      value = value << 4 | digit;
    }
    out[i] = value;
  }
  return text[size * 2] == '\0';
}

void PageLogin::begin() {
  prefs_.begin(NAMESPACE, false /* read-write */);
  // isKey() first, because reading a missing key logs an error.
  hasPassword_ = prefs_.isKey(KEY_HASH) && prefs_.isKey(KEY_SALT) && prefs_.isKey(KEY_SIGNING) &&
                 prefs_.getBytes(KEY_HASH, hash_, sizeof(hash_)) == sizeof(hash_) &&
                 prefs_.getBytes(KEY_SALT, salt_, sizeof(salt_)) == sizeof(salt_) &&
                 prefs_.getBytes(KEY_SIGNING, key_, sizeof(key_)) == sizeof(key_);
}

void PageLogin::hash(const char* password, uint8_t* out) const {
  mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256, reinterpret_cast<const unsigned char*>(password),
                                strlen(password), salt_, sizeof(salt_), PAGE_PASSWORD_ROUNDS,
                                sizeof(hash_), out);
}

void PageLogin::sign(const uint8_t* nonce, uint8_t* out) const {
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), key_, sizeof(key_), nonce,
                  NONCE_SIZE, out);
}

void PageLogin::setPassword(const char* password) {
  // A password is only ever set with the radio on (setup mode's network or
  // the home one), which is what makes these truly random.
  esp_fill_random(salt_, sizeof(salt_));
  esp_fill_random(key_, sizeof(key_));

  const unsigned long startMs = millis();
  hash(password, hash_);
  const unsigned long tookMs = millis() - startMs;

  prefs_.putBytes(KEY_SALT, salt_, sizeof(salt_));
  prefs_.putBytes(KEY_HASH, hash_, sizeof(hash_));
  prefs_.putBytes(KEY_SIGNING, key_, sizeof(key_));
  hasPassword_ = true;
  failures_ = 0;
  locked_ = false;

  Serial.printf("Login: new password set (checking one takes %lu ms); every browser logged out\n",
                tookMs);
}

PageLogin::Check PageLogin::check(const char* password, unsigned long nowMs) {
  if (locked_) {
    if (nowMs - lockedSinceMs_ < PAGE_LOGIN_LOCKOUT_MS) return Check::LockedOut;
    locked_ = false;
    failures_ = 0;
  }
  if (!hasPassword_) return Check::Wrong;

  uint8_t typed[sizeof(hash_)];
  hash(password, typed);
  if (sameBytes(typed, hash_, sizeof(hash_))) {
    failures_ = 0;
    return Check::Right;
  }

  if (++failures_ >= PAGE_LOGIN_MAX_FAILURES) {
    locked_ = true;
    lockedSinceMs_ = nowMs;
    Serial.printf("Login: %d wrong passwords in a row, refusing logins for %lu s\n", failures_,
                  PAGE_LOGIN_LOCKOUT_MS / 1'000);
  }
  return Check::Wrong;
}

String PageLogin::newLogin() const {
  uint8_t nonce[NONCE_SIZE];
  esp_fill_random(nonce, sizeof(nonce));
  uint8_t mac[MAC_SIZE];
  sign(nonce, mac);
  return toHex(nonce, sizeof(nonce)) + toHex(mac, sizeof(mac));
}

bool PageLogin::isLogin(const String& login) const {
  if (!hasPassword_ || login.length() != (NONCE_SIZE + MAC_SIZE) * 2) return false;

  uint8_t nonce[NONCE_SIZE], mac[MAC_SIZE], expected[MAC_SIZE];
  if (!fromHex(login.substring(0, NONCE_SIZE * 2).c_str(), nonce, sizeof(nonce)) ||
      !fromHex(login.c_str() + NONCE_SIZE * 2, mac, sizeof(mac))) {
    return false;
  }
  sign(nonce, expected);
  return sameBytes(mac, expected, sizeof(mac));
}

String PageLogin::cookieFor(const String& login) {
  // HttpOnly: the page's own script never needs it. SameSite=Lax: another
  // site can link to the settings, but can't post to them with it.
  String out = String(COOKIE) + "=" + login + "; Path=/; HttpOnly; SameSite=Lax; Max-Age=";
  out += login.isEmpty() ? 0UL : PAGE_LOGIN_DAYS * 24 * 60 * 60;
  return out;
}

String PageLogin::loginIn(const String& cookies) {
  // "theme=dark; login=0123...; other=1"
  const String prefix = String(COOKIE) + "=";
  int start = 0;
  while (start < static_cast<int>(cookies.length())) {
    int end = cookies.indexOf(';', start);
    if (end < 0) end = cookies.length();
    String part = cookies.substring(start, end);
    part.trim();
    if (part.startsWith(prefix)) return part.substring(prefix.length());
    start = end + 1;
  }
  return "";
}
