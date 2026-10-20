#include "net/direct_api.h"

#include <ArduinoJson.h>

#include "config.h"
#include "net/own_address.h"
#include "net/protocol_json.h"

void DirectApi::begin(HeatPumpLink& heatPump, const UnitSettings& settings) {
  if (started_) return;
  started_ = true;
  heatPump_ = &heatPump;
  settings_ = &settings;

  // Browsers only from this controller's own page; see net/own_address.h.
  server_.onValidateHttpHeader(
      [this](String header, String value) {
        return !header.equalsIgnoreCase("Origin") || isOwnOrigin(value, settings_->name);
      },
      nullptr, 0);

  server_.onEvent([this](uint8_t client, WStype_t type, uint8_t* payload, size_t length) {
    onEvent(client, type, payload, length);
  });
  server_.enableHeartbeat(DIRECT_API_PING_INTERVAL_MS, DIRECT_API_PONG_TIMEOUT_MS,
                          DIRECT_API_MISSED_PONGS);
  server_.begin();

  Serial.printf("Control API: ws://%s.local:%u/\n", settings.name, DIRECT_API_PORT);
}

void DirectApi::tick() {
  if (started_) server_.loop();
}

void DirectApi::publish() {
  if (!started_ || server_.connectedClients() == 0) return;
  String json = stateJson();
  server_.broadcastTXT(json);
}

void DirectApi::onEvent(uint8_t client, WStype_t type, const uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED: {
      Serial.printf("Control API: %s connected\n", server_.remoteIP(client).toString().c_str());
      String json = stateJson();
      server_.sendTXT(client, json);
      break;
    }
    case WStype_TEXT:
      handleCommand(client, payload, length);
      break;
    default:
      break;
  }
}

void DirectApi::handleCommand(uint8_t client, const uint8_t* payload, size_t length) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, length) || !doc.is<JsonObject>()) {
    sendError(client, "Not a JSON object.");
    return;
  }

  HeatPumpCommand command;
  if (const char* error =
          protocol_json::readCommand(doc.as<JsonObjectConst>(), *settings_, command)) {
    sendError(client, error);
    return;
  }

  Serial.printf("Control API: change from %s: %.*s\n", server_.remoteIP(client).toString().c_str(),
                static_cast<int>(length), reinterpret_cast<const char*>(payload));

  if (!heatPump_->state().connected) {
    sendError(client, "The heat pump isn't connected.");
  } else if (!heatPump_->apply(command)) {
    sendError(client, "The heat pump didn't take the change.");
  }
}

void DirectApi::sendError(uint8_t client, const char* message) {
  JsonDocument doc;
  doc[KEY_ERROR] = message;
  String json;
  serializeJson(doc, json);
  server_.sendTXT(client, json);
}

String DirectApi::stateJson() const {
  const UnitSettings& settings = *settings_;
  const HeatPumpState& state = heatPump_->state();

  JsonDocument doc;
  doc[KEY_NAME] = settings.name;
  doc[KEY_CONNECTED] = state.connected;
  doc[KEY_TEMPERATURE_UNIT] = settings.useFahrenheit ? "F" : "C";
  doc[KEY_MIN_TEMP] = settings.minTemp;
  doc[KEY_MAX_TEMP] = settings.maxTemp;
  doc[KEY_TEMP_STEP] = settings.tempStep;

  if (state.connected) {
    protocol_json::writeSettings(doc.as<JsonObject>(), state, settings);
    protocol_json::writeStatus(doc.as<JsonObject>(), state, settings);
  }

  String json;
  serializeJson(doc, json);
  return json;
}
