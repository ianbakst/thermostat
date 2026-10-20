#pragma once
#include <WebSocketsServer.h>

#include "climate/heat_pump.h"
#include "protocol/mini_split_protocol.h"
#include "storage/settings_store.h"

// The control API: a WebSocket on DIRECT_API_PORT, speaking the JSON that
// mini_split_protocol.h describes. The page at / drives the unit through it,
// and so will a panel that has no broker to go through.
//
// It runs from loop(), like the heat pump link, so a command goes to the unit
// straight from the message that asked for it with nothing to lock. While the
// unit acknowledges a change (a second or two), nothing else is served.
class DirectApi {
 public:
  // Starts listening, once the network is up. Only the first call does
  // anything. Both are used for every message, so they must outlive this.
  void begin(HeatPumpLink& heatPump, const UnitSettings& settings);

  // Call from loop(). Handles connections and whatever clients have sent.
  void tick();

  // Sends the current state to every client. Call whenever it changes.
  void publish();

 private:
  void onEvent(uint8_t client, WStype_t type, const uint8_t* payload, size_t length);
  void handleCommand(uint8_t client, const uint8_t* payload, size_t length);
  void sendError(uint8_t client, const char* message);
  String stateJson() const;

  WebSocketsServer server_{DIRECT_API_PORT};
  HeatPumpLink* heatPump_ = nullptr;
  const UnitSettings* settings_ = nullptr;
  bool started_ = false;
};
