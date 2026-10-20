#include "net/protocol_json.h"

#include "climate/temperature_units.h"

namespace protocol_json {

void writeSettings(JsonObject out, const HeatPumpState& state, const UnitSettings& settings) {
  out[KEY_MODE] = MODE_VALUES[static_cast<int>(state.mode)];
  out[KEY_TEMPERATURE] = toReported(state.targetTemp, settings.useFahrenheit);
  out[KEY_FAN] = FAN_VALUES[static_cast<int>(state.fan)];
  out[KEY_VANE] = VANE_VALUES[static_cast<int>(state.vane)];
  out[KEY_WIDE_VANE] = WIDE_VANE_VALUES[static_cast<int>(state.wideVane)];
}

void writeStatus(JsonObject out, const HeatPumpState& state, const UnitSettings& settings) {
  if (std::isnan(state.roomTemp)) {
    out[KEY_ROOM_TEMPERATURE] = nullptr;
  } else {
    out[KEY_ROOM_TEMPERATURE] = toReported(state.roomTemp, settings.useFahrenheit);
  }
  out[KEY_ACTION] = ACTION_VALUES[static_cast<int>(state.action)];
  out[KEY_COMPRESSOR_FREQUENCY] = state.compressorHz;
}

// Reads one of the vocabulary's settings into `out`. Returns false if it's
// there but isn't one of the protocol's words; absent is fine.
template <typename Enum>
static bool readChoice(JsonObjectConst in, const char* key, const char* const* values, int count,
                       std::optional<Enum>& out) {
  if (in[key].isNull()) return true;
  const int index = indexOf(values, count, in[key] | "");
  if (index < 0) return false;
  out = static_cast<Enum>(index);
  return true;
}

const char* readCommand(JsonObjectConst in, const UnitSettings& settings,
                        HeatPumpCommand& command) {
  if (!readChoice(in, KEY_MODE, MODE_VALUES, HVAC_MODE_COUNT, command.mode) ||
      !readChoice(in, KEY_FAN, FAN_VALUES, FAN_MODE_COUNT, command.fan) ||
      !readChoice(in, KEY_VANE, VANE_VALUES, VANE_MODE_COUNT, command.vane) ||
      !readChoice(in, KEY_WIDE_VANE, WIDE_VANE_VALUES, WIDE_VANE_MODE_COUNT, command.wideVane)) {
    return "Unknown value: see mini_split_protocol.h for the words each setting takes.";
  }

  if (!in[KEY_TEMPERATURE].isNull()) {
    const float target = in[KEY_TEMPERATURE] | NAN;
    if (std::isnan(target)) return "The setpoint isn't a number.";
    if (target < settings.minTemp || target > settings.maxTemp) {
      return "Setpoint outside this unit's limits.";
    }
    command.targetTemp = toCelsius(target, settings.useFahrenheit);
  }

  if (command.empty()) return "Nothing to change.";
  return nullptr;
}

}  // namespace protocol_json
