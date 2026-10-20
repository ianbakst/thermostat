#include "climate/heat_pump.h"

#include <Arduino.h>

#include <cstring>

#include "config.h"

// The heat pump's own words, in the order of our enums, so a lookup is the
// same indexOf() both firmwares use on the wire values.
//
// Power is separate from mode on the control board: "HEAT" with the power off
// still reports as HEAT, so Off is decided before the mode is read, and the
// table's first entry is never matched.
static const char* const HP_MODE_VALUES[HVAC_MODE_COUNT] = {"", "HEAT", "COOL", "AUTO", "DRY", "FAN"};

// "QUIET" is what the remote calls the slowest speed, which HA calls diffuse.
static const char* const HP_FAN_VALUES[FAN_MODE_COUNT] = {"AUTO", "QUIET", "1", "2", "3", "4"};

// These two match the protocol's values exactly, so they're listed once in the
// shared header: VANE_VALUES ("AUTO", "1".."5", "SWING") and WIDE_VANE_VALUES
// ("<<" .. "SWING") are the strings the library uses as well.

// An unknown temperature is NAN, and NAN never equals itself, so comparing
// states field by field needs this: two unknowns are the same thing.
static bool sameTemp(float a, float b) { return (isnan(a) && isnan(b)) || a == b; }

// Everything but compressorHz, which would make every second a change.
static bool same(const HeatPumpState& a, const HeatPumpState& b) {
  return a.connected == b.connected && a.mode == b.mode && a.fan == b.fan && a.vane == b.vane &&
         a.wideVane == b.wideVane && a.action == b.action && sameTemp(a.targetTemp, b.targetTemp) &&
         sameTemp(a.roomTemp, b.roomTemp);
}

void HeatPumpState::describe(char* out, size_t size) const {
  if (!connected) {
    snprintf(out, size, "not connected");
    return;
  }

  char room[8] = "--";
  if (!isnan(roomTemp)) snprintf(room, sizeof(room), "%.1f", roomTemp);

  snprintf(out, size, "mode %s, target %.1f°C, room %s°C, fan %s, vane %s, wideVane %s, %s",
           MODE_VALUES[static_cast<int>(mode)], targetTemp, room,
           FAN_VALUES[static_cast<int>(fan)], VANE_VALUES[static_cast<int>(vane)],
           WIDE_VANE_VALUES[static_cast<int>(wideVane)], ACTION_VALUES[static_cast<int>(action)]);
}

void HeatPumpLink::begin() {
  Serial.printf("Heat pump: connecting over CN105 (RX %d, TX %d, %d baud)\n", CN105_RX_PIN,
                CN105_TX_PIN, CN105_BAUD);
  // The library opens the port as 8E1 and sends the connect packet. Anything
  // other than success means the unit didn't answer; tick() retries.
  started_ = hp_.connect(&Serial1, CN105_BAUD, CN105_RX_PIN, CN105_TX_PIN);
  lastAttemptMs_ = millis();
  if (!started_) Serial.println("Heat pump: no answer yet, will retry");
}

bool HeatPumpLink::apply(const HeatPumpCommand& command) {
  if (!hp_.isConnected()) return false;

  // Until the unit has reported its settings, the library holds null
  // pointers for them, and there's nothing to start the change from.
  const heatpumpSettings current = hp_.getSettings();
  if (!current.power || !current.mode || !current.fan || !current.vane || !current.wideVane) {
    return false;
  }

  // The library sends every setting where what it was last asked for differs
  // from what the unit reports, and "last asked for" is only brought up to
  // date on the first read. Without this, anything changed with the remote
  // since then would be undone along with the change asked for here.
  hp_.setSettings(current);

  if (command.mode) {
    const bool on = *command.mode != HvacMode::Off;
    hp_.setPowerSetting(on);
    if (on) hp_.setModeSetting(HP_MODE_VALUES[static_cast<int>(*command.mode)]);
  }
  if (command.targetTemp) {
    // Half degrees are the finest any unit takes. See HP_MIN_SETPOINT_C for
    // why the clamp isn't optional.
    const float halves = roundf(*command.targetTemp * 2) / 2;
    hp_.setTemperature(constrain(halves, HP_MIN_SETPOINT_C, HP_MAX_SETPOINT_C));
  }
  if (command.fan) hp_.setFanSpeed(HP_FAN_VALUES[static_cast<int>(*command.fan)]);
  if (command.vane) hp_.setVaneSetting(VANE_VALUES[static_cast<int>(*command.vane)]);
  if (command.wideVane) {
    hp_.setWideVaneSetting(WIDE_VANE_VALUES[static_cast<int>(*command.wideVane)]);
  }

  const bool accepted = hp_.update();
  Serial.printf("Heat pump: change %s\n", accepted ? "accepted" : "not acknowledged");
  return accepted;
}

bool HeatPumpLink::tick(unsigned long nowMs) {
  const HeatPumpState before = state_;

  if (!hp_.isConnected()) {
    // Back off: wait longer after each failure, up to HP_RETRY_MAX_MS, then
    // keep trying at that interval for as long as it takes.
    const unsigned long wait = retryIntervalMs_ ? retryIntervalMs_ : HP_RETRY_INTERVAL_MS;
    if (nowMs - lastAttemptMs_ >= wait) {
      lastAttemptMs_ = nowMs;
      retryIntervalMs_ = min(wait * 2, HP_RETRY_MAX_MS);
      if (hp_.connect(&Serial1, CN105_BAUD, CN105_RX_PIN, CN105_TX_PIN)) {
        Serial.println("Heat pump: connected");
        retryIntervalMs_ = 0;
        started_ = true;
      }
    }
    state_.connected = false;
  } else if (nowMs - lastSyncMs_ >= HP_SYNC_INTERVAL_MS) {
    lastSyncMs_ = nowMs;
    hp_.sync();  // sends a request and reads whatever has arrived
    if (hp_.isConnected()) readState();
  }

  return !same(before, state_);
}

void HeatPumpLink::readState() {
  const heatpumpSettings settings = hp_.getSettings();
  const heatpumpStatus status = hp_.getStatus();

  state_.connected = true;

  // Off wins over whatever mode the board is still holding.
  const bool off = settings.power && strcmp(settings.power, "OFF") == 0;
  const int mode = off ? 0 : indexOf(HP_MODE_VALUES, HVAC_MODE_COUNT, settings.mode);
  state_.mode = static_cast<HvacMode>(mode >= 0 ? mode : 0);

  const int fan = indexOf(HP_FAN_VALUES, FAN_MODE_COUNT, settings.fan);
  if (fan >= 0) state_.fan = static_cast<FanMode>(fan);

  const int vane = indexOf(VANE_VALUES, VANE_MODE_COUNT, settings.vane);
  if (vane >= 0) state_.vane = static_cast<VaneMode>(vane);

  const int wideVane = indexOf(WIDE_VANE_VALUES, WIDE_VANE_MODE_COUNT, settings.wideVane);
  if (wideVane >= 0) state_.wideVane = static_cast<WideVaneMode>(wideVane);

  state_.targetTemp = settings.temperature;
  // The unit reports 0 before its sensor has been read, which is not a room
  // temperature; say "unknown" instead so nothing downstream displays 0°C.
  state_.roomTemp = status.roomTemperature > 0 ? status.roomTemperature : NAN;
  state_.compressorHz = status.compressorFrequency;

  // What it's doing, as opposed to what it's set to: `operating` is false once
  // the room has reached the setpoint, which is Idle rather than Heating.
  switch (state_.mode) {
    case HvacMode::Off:     state_.action = HvacAction::Off; break;
    case HvacMode::Dry:     state_.action = HvacAction::Drying; break;
    case HvacMode::FanOnly: state_.action = HvacAction::Fan; break;
    case HvacMode::Heat:    state_.action = status.operating ? HvacAction::Heating : HvacAction::Idle; break;
    case HvacMode::Cool:    state_.action = status.operating ? HvacAction::Cooling : HvacAction::Idle; break;
    case HvacMode::Auto:
      // In Auto the board says it's working but not which way, so the room
      // temperature against the setpoint is the only clue. With no reading
      // yet, Idle is the honest answer rather than a guess.
      if (!status.operating || isnan(state_.roomTemp)) {
        state_.action = HvacAction::Idle;
      } else {
        state_.action =
            state_.roomTemp < state_.targetTemp ? HvacAction::Heating : HvacAction::Cooling;
      }
      break;
  }
}
