#pragma once
#include <cmath>

#include "config.h"

namespace tracked_detail {

template <typename T>
inline bool sameValue(T a, T b) {
  return a == b;
}

// Temperatures: float rounding, and unknown (NAN) equals unknown.
inline bool sameValue(float a, float b) {
  return std::fabs(a - b) < 0.01f || (std::isnan(a) && std::isnan(b));
}

}  // namespace tracked_detail

// Sets `shown` to `value`. Returns true if that changed it. For values the
// panel only ever receives, with no change of its own to wait for.
template <typename T>
bool assignIfChanged(T& shown, T value) {
  if (tracked_detail::sameValue(shown, value)) return false;
  shown = value;
  return true;
}

// One value the panel shows that the mini-split also reports back: the
// setpoint, the mode or the fan speed.
//
// A change the user makes shows at once, and is then held: until the unit
// reports that same value back, reports still carrying the old one are ignored,
// so the screen doesn't flick back while the unit catches up. After
// PENDING_HOLD_MS the unit's latest report wins regardless — which is what
// happens if the unit refused the change.
//
// The value itself lives in the ClimateState the UI is handed, so this binds to
// that field once at construction rather than being passed it on every call.
template <typename T>
class Tracked {
 public:
  explicit Tracked(T& shown) : shown_(shown) {}

  Tracked(const Tracked&) = delete;
  Tracked& operator=(const Tracked&) = delete;

  // The user asked for `value`. Returns true if that changed what's shown.
  bool request(T value, unsigned long nowMs) {
    pending_ = true;
    heldSinceMs_ = nowMs;
    return assignIfChanged(shown_, value);
  }

  // The unit reported `value`. Returns true if that changed what's shown.
  bool report(T value) {
    reported_ = value;
    hasReport_ = true;
    if (pending_) {
      // Still the old value: the unit hasn't caught up, so leave the screen be.
      if (!tracked_detail::sameValue(shown_, value)) return false;
      pending_ = false;
    }
    return assignIfChanged(shown_, value);
  }

  // Gives up holding after PENDING_HOLD_MS, showing the unit's latest report.
  // Returns true if that changed what's shown.
  bool expire(unsigned long nowMs) {
    if (!pending_ || nowMs - heldSinceMs_ < PENDING_HOLD_MS) return false;
    pending_ = false;
    return hasReport_ && assignIfChanged(shown_, reported_);
  }

  // Whether the unit has ever reported this value.
  bool hasReport() const { return hasReport_; }

  // When the hold started, and a way to restart it — for a change that's sent
  // later than it was made (the setpoint waits for the taps to pause, and
  // should be held from when it went out).
  unsigned long heldSinceMs() const { return heldSinceMs_; }
  void holdFrom(unsigned long nowMs) { heldSinceMs_ = nowMs; }

 private:
  T& shown_;
  T reported_{};
  bool pending_ = false;
  bool hasReport_ = false;
  unsigned long heldSinceMs_ = 0;
};
