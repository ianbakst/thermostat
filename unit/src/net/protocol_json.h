#pragma once
#include <ArduinoJson.h>

#include "climate/heat_pump.h"
#include "storage/settings_store.h"

// The heat pump's state, and changes to it, in the JSON that
// mini_split_protocol.h describes. The control API and MQTT both speak it, so
// a setting reads the same and is checked the same way whichever way it
// arrives.
//
// Temperatures are in the unit chosen during setup on both sides; the heat
// pump's own °C stays below this.
namespace protocol_json {

// mode, temperature, fan, vane and wideVane: what MQTT retains as `settings`.
// Only meaningful while the heat pump is connected.
void writeSettings(JsonObject out, const HeatPumpState& state, const UnitSettings& settings);

// What the unit is doing rather than what it's set to: roomTemperature (null
// until the first reading), action and compressorFrequency. MQTT's `state` is
// these plus the settings.
void writeStatus(JsonObject out, const HeatPumpState& state, const UnitSettings& settings);

// Reads the settings a client asked to change, in the values the /set topics
// take, into `command`. Returns null if that makes a change, or else what's
// wrong with it, in words for whoever sent it.
const char* readCommand(JsonObjectConst in, const UnitSettings& settings, HeatPumpCommand& command);

}  // namespace protocol_json
