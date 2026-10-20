#pragma once
//
// The link to the indoor unit over CN105, and the one place that knows the
// heat pump's own vocabulary.
//
// Mitsubishi's control board talks about "HEAT" with the power off, fan speed
// "2" and vane "SWING". Everything above this class works in the terms the
// panel and Home Assistant use (heat_cool, middle, Swing), which the shared
// protocol header defines. The translation lives here and nowhere else.
//
// The framing underneath — packet types, checksums, the 2400-baud 8E1 link —
// is SwiCago's HeatPump library. That library is GPL-3.0, so it sits behind
// this interface: nothing else in the firmware includes it, and replacing it
// with our own implementation of the protocol would mean rewriting only this
// pair of files. See README.md.
//

#include <HeatPump.h>

#include <cmath>
#include <optional>

#include "protocol/mini_split_protocol.h"

// What the indoor unit says it is doing. Temperatures are in °C, as the unit
// reports them; converting to what a panel displays happens later, when it's
// published.
struct HeatPumpState {
  bool connected = false;  // false until the unit answers, and after it stops

  HvacMode mode = HvacMode::Off;
  FanMode fan = FanMode::Auto;
  VaneMode vane = VaneMode::Auto;
  WideVaneMode wideVane = WideVaneMode::Center;
  HvacAction action = HvacAction::Off;

  float targetTemp = NAN;
  float roomTemp = NAN;  // NAN until the first reading arrives

  // How fast the compressor is running, in Hz; 0 when it's stopped. Home
  // Assistant gets it as an attribute, as it did from mitsubishi2mqtt. It
  // moves all the time while the unit runs, so it isn't a change by itself:
  // it goes out with whatever is published next.
  int compressorHz = 0;

  // One line for the log and the unit's page, in the panel's words:
  // "mode heat, target 22.0°C, room 20.5°C, fan auto, vane AUTO, ...".
  void describe(char* out, size_t size) const;
};

// A change to make, in the same terms as HeatPumpState: our vocabulary, and
// °C. Anything left empty stays as the unit has it.
struct HeatPumpCommand {
  std::optional<HvacMode> mode;  // Off turns the unit off; anything else turns it on
  std::optional<float> targetTemp;
  std::optional<FanMode> fan;
  std::optional<VaneMode> vane;
  std::optional<WideVaneMode> wideVane;

  bool empty() const { return !mode && !targetTemp && !fan && !vane && !wideVane; }
};

class HeatPumpLink {
 public:
  // Opens the serial port and starts connecting. Returns immediately: the
  // unit answers in its own time, and tick() reports when it has.
  void begin();

  // Call from loop(). Talks to the unit at most every HP_SYNC_INTERVAL_MS, and
  // returns true when anything in state() has changed since the last call —
  // including the unit appearing or going away.
  bool tick(unsigned long nowMs);

  const HeatPumpState& state() const { return state_; }

  // Sends a change to the unit, and waits for it to say it took it. That
  // blocks for a second or two: the unit wants a pause between packets, and
  // the link runs at 2400 baud. Returns false if the unit isn't connected,
  // hasn't reported its settings yet, or didn't acknowledge.
  //
  // state() doesn't change here. It catches up when the unit next reports
  // its settings, a few seconds later, so it's only ever what the unit says.
  bool apply(const HeatPumpCommand& command);

 private:
  void readState();

  HeatPump hp_;
  HeatPumpState state_;
  bool started_ = false;
  unsigned long lastSyncMs_ = 0;
  unsigned long lastAttemptMs_ = 0;
  unsigned long retryIntervalMs_ = 0;  // 0 until the first failure
};
