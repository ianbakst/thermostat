#pragma once
//
// The contract between this repo's two firmwares, and with Home Assistant:
// the MQTT topics, and the direct API further down that works without a
// broker.
//
// The mini-split's controller publishes its state here and listens for
// changes; the panel does the opposite. HA reads the same topics, which is
// why the vocabulary is HA's: off / heat / cool / heat_cool / dry / fan_only
// for modes, and auto / diffuse / low / middle / medium / high for the fan.
//
// For a unit named <name>:
//
//   homeassistant/climate/<name>/config    the unit's HA discovery config,
//                                          RETAINED, which is how HA finds
//                                          it. The panel subscribes to
//                                          homeassistant/climate/+/config to
//                                          list the units to pick from; the
//                                          chosen one's gives its temperature
//                                          unit, setpoint limits and step
//   mitsubishi2mqtt/<name>/availability    "online" / "offline", RETAINED
//   mitsubishi2mqtt/<name>/settings        {"temperature":77,"fan":"auto",...}
//                                          RETAINED, sent after every change
//   mitsubishi2mqtt/<name>/state           the same plus "roomTemperature",
//                                          "action" and "compressorFrequency",
//                                          every 30s
//
//   mitsubishi2mqtt/<name>/mode/set        off, heat, cool, heat_cool, dry, fan_only
//   mitsubishi2mqtt/<name>/temp/set        the setpoint, e.g. 72
//   mitsubishi2mqtt/<name>/fan/set         auto, diffuse, low, middle, medium, high
//   mitsubishi2mqtt/<name>/vane/set        AUTO, 1-5, SWING (the vertical louvre,
//                                          which HA shows as the swing mode)
//   mitsubishi2mqtt/<name>/wideVane/set    <<, <, |, >, >>, <>, SWING (the
//                                          horizontal louvre, which recent HA
//                                          shows as the horizontal swing mode)
//   mitsubishi2mqtt/<name>/remote_temp/set a room temperature measured
//                                          elsewhere, for the unit to use
//                                          instead of its own sensor. This
//                                          repo's controller doesn't take it
//                                          yet
//
// This repo's controller says "online" only while it's talking to the indoor
// unit, and the broker says "offline" for it (its last will) if it drops off.
// mitsubishi2mqtt said "online" whenever it was on the broker.
//
// **Retained matters.** The panel shows real state the moment it connects
// because `settings` and `availability` are retained and arrive immediately;
// `state`, which isn't, only turns up on the next periodic publish. A
// controller that retains nothing leaves a panel blank for up to 30 seconds
// after every reconnection.
//
// The names come from mitsubishi2mqtt, which the mini-splits ran first (see
// unit/README.md). Keeping them means a panel works with either firmware and
// Home Assistant needs no changes. The actively maintained fork of that
// project renamed `wideVane/set` to `wide-vane/set`, dropped `settings`
// entirely and stopped retaining anything, so it is NOT compatible with this
// contract; docs/protocol.md has the details.
//

#include <cstdint>
#include <cstring>

// ---------------------------------------------------------------------------
// Topics
// ---------------------------------------------------------------------------
// Macros, not constexpr: these get pasted into string literals to build the
// full topics, which only the preprocessor can do.
#define MINI_SPLIT_TOPIC_PREFIX "mitsubishi2mqtt"
#define HA_DISCOVERY_PREFIX "homeassistant"

inline constexpr const char* DISCOVERY_PREFIX = HA_DISCOVERY_PREFIX "/climate/";
inline constexpr const char* DISCOVERY_SUFFIX = "/config";
inline constexpr const char* DISCOVERY_WILDCARD = HA_DISCOVERY_PREFIX "/climate/+/config";

// What a unit publishes.
inline constexpr const char* TOPIC_AVAILABILITY = "availability";
inline constexpr const char* TOPIC_SETTINGS = "settings";
inline constexpr const char* TOPIC_STATE = "state";

// What it listens on. The panel builds "<prefix>/<name>/<this>".
inline constexpr const char* TOPIC_MODE_SET = "mode/set";
inline constexpr const char* TOPIC_TEMP_SET = "temp/set";
inline constexpr const char* TOPIC_FAN_SET = "fan/set";
inline constexpr const char* TOPIC_VANE_SET = "vane/set";
inline constexpr const char* TOPIC_WIDE_VANE_SET = "wideVane/set";
inline constexpr const char* TOPIC_REMOTE_TEMP_SET = "remote_temp/set";

inline constexpr const char* PAYLOAD_ONLINE = "online";
inline constexpr const char* PAYLOAD_OFFLINE = "offline";

// ---------------------------------------------------------------------------
// The JSON keys in `settings` and `state`
// ---------------------------------------------------------------------------
inline constexpr const char* KEY_TEMPERATURE = "temperature";
inline constexpr const char* KEY_MODE = "mode";
inline constexpr const char* KEY_FAN = "fan";
inline constexpr const char* KEY_VANE = "vane";
inline constexpr const char* KEY_WIDE_VANE = "wideVane";
// Unknown until the unit's sensor has been read: this repo's controller sends
// null, and mitsubishi2mqtt sent 0 or left `state` out altogether. A reader
// takes anything that isn't above 0 as unknown.
inline constexpr const char* KEY_ROOM_TEMPERATURE = "roomTemperature";
inline constexpr const char* KEY_ACTION = "action";
inline constexpr const char* KEY_COMPRESSOR_FREQUENCY = "compressorFrequency";  // Hz, 0 when stopped

// ---------------------------------------------------------------------------
// The direct API: a WebSocket on the controller itself
// ---------------------------------------------------------------------------
// ws://<name>.local:81/, for the controller's own page now and for panels
// with no broker later. Text messages, JSON, in the same words as MQTT.
//
// The controller sends its whole state when a client connects, and again
// after every change:
//
//   {"name":"OfficeHVAC","connected":true,
//    "temperature_unit":"F","min_temp":50,"max_temp":90,"temp_step":1,
//    "mode":"heat","temperature":72,"fan":"auto","vane":"AUTO","wideVane":"|",
//    "roomTemperature":70,"action":"heating","compressorFrequency":38}
//
// "connected" is whether it's talking to the indoor unit; without that, the
// settings after the limits are left out, since there's nothing real to
// report. The limits are the ones in its discovery config, under the same
// names. roomTemperature is null until the first reading.
//
// A client sends whichever settings it wants changed, in the values the /set
// topics take:
//
//   {"mode":"cool"}    {"temperature":73,"fan":"low"}
//
// One that can't be made gets {"error":"..."} back, to that client only. One
// that can shows up in the next state message, once the indoor unit has
// confirmed it, a few seconds later: state is only ever what the unit
// reports.
//
// A browser is only let in from the controller's own page (by its Origin
// header), so another website open in the same browser can't drive the unit.
inline constexpr uint16_t DIRECT_API_PORT = 81;
inline constexpr const char* KEY_NAME = "name";
inline constexpr const char* KEY_CONNECTED = "connected";
inline constexpr const char* KEY_TEMPERATURE_UNIT = "temperature_unit";
inline constexpr const char* KEY_MIN_TEMP = "min_temp";
inline constexpr const char* KEY_MAX_TEMP = "max_temp";
inline constexpr const char* KEY_TEMP_STEP = "temp_step";
inline constexpr const char* KEY_ERROR = "error";

// ---------------------------------------------------------------------------
// The vocabulary, as enums and the strings they travel as
// ---------------------------------------------------------------------------
// Each enum sits next to its own table so the two can't drift apart: the
// order of one is the order of the other. For the panel, that order is also
// the order options appear in its pickers.
enum class HvacMode : uint8_t { Off, Heat, Cool, Auto, Dry, FanOnly };
constexpr int HVAC_MODE_COUNT = 6;
inline constexpr const char* const MODE_VALUES[HVAC_MODE_COUNT] = {"off",       "heat", "cool",
                                                                   "heat_cool", "dry",  "fan_only"};

enum class FanMode : uint8_t { Auto, Quiet, Low, Middle, Medium, High };
constexpr int FAN_MODE_COUNT = 6;
inline constexpr const char* const FAN_VALUES[FAN_MODE_COUNT] = {"auto",   "diffuse", "low",
                                                                 "middle", "medium",  "high"};

// What the unit is doing right now, which can differ from its mode: in Heat
// it's Heating while the compressor runs and Idle once the room is warm
// enough. The same values as Home Assistant's hvac_action.
enum class HvacAction : uint8_t { Off, Idle, Heating, Cooling, Drying, Fan };
constexpr int HVAC_ACTION_COUNT = 6;
inline constexpr const char* const ACTION_VALUES[HVAC_ACTION_COUNT] = {
    "off", "idle", "heating", "cooling", "drying", "fan"};

// The louvres. Vertical is what Home Assistant shows as the climate entity's
// swing mode. mitsubishi2mqtt didn't advertise horizontal to HA at all; this
// repo's controller offers it as the horizontal swing mode, which HA versions
// older than that feature ignore.
//
// Vane 1 is the flattest position and 5 points furthest down. Wide vane runs
// left to right, and Spread ("<>") splits the airflow both ways.
enum class VaneMode : uint8_t { Auto, Pos1, Pos2, Pos3, Pos4, Pos5, Swing };
constexpr int VANE_MODE_COUNT = 7;
inline constexpr const char* const VANE_VALUES[VANE_MODE_COUNT] = {"AUTO", "1",    "2",    "3",
                                                                   "4",    "5",    "SWING"};

enum class WideVaneMode : uint8_t { FarLeft, Left, Center, Right, FarRight, Spread, Swing };
constexpr int WIDE_VANE_MODE_COUNT = 7;
inline constexpr const char* const WIDE_VANE_VALUES[WIDE_VANE_MODE_COUNT] = {
    "<<", "<", "|", ">", ">>", "<>", "SWING"};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
// Position of `value` in one of the tables above, or -1. Both firmwares turn
// a wire value into an enum this way.
inline int indexOf(const char* const* values, int count, const char* value) {
  if (!value) return -1;
  for (int i = 0; i < count; i++) {
    if (strcmp(values[i], value) == 0) return i;
  }
  return -1;
}

// Pulls <name> out of "homeassistant/climate/<name>/config". Returns false if
// the topic isn't one of those, or if the name doesn't fit in `out`.
inline bool discoveryUnitName(const char* topic, char* out, size_t size) {
  const size_t prefixLength = strlen(DISCOVERY_PREFIX);
  if (strncmp(topic, DISCOVERY_PREFIX, prefixLength) != 0) return false;
  const char* suffix = strstr(topic + prefixLength, DISCOVERY_SUFFIX);
  if (!suffix || suffix[strlen(DISCOVERY_SUFFIX)] != '\0') return false;

  const size_t length = static_cast<size_t>(suffix - (topic + prefixLength));
  if (length == 0 || length >= size) return false;
  memcpy(out, topic + prefixLength, length);
  out[length] = '\0';
  return true;
}
