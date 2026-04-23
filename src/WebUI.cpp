#include "WebUI.h"

WebSocketMessageHandler WebUI::messageHandler = nullptr;

static void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WS] Client #%u disconnected\n", num);
      break;
    case WStype_CONNECTED:
      Serial.printf("[WS] Client #%u connected\n", num);
      break;
    case WStype_TEXT:
      Serial.printf("[WS] Client #%u sent: %s\n", num, payload);
      if (WebUI::messageHandler != nullptr) {
        String msg = String((char*)payload);
        WebUI::messageHandler(num, msg);
      }
      break;
    default:
      break;
  }
}

void WebUI::begin() {
  ws.onEvent(webSocketEvent);
  ws.begin();
  Serial.println("[WS] WebSocket server started on port 81");
}

void WebUI::setMessageHandler(WebSocketMessageHandler handler) {
  messageHandler = handler;
}

void WebUI::sendToClient(uint8_t num, String message) {
  ws.sendTXT(num, message);
}

void WebUI::broadcastState(bool running, bool sequence, unsigned long remaining, unsigned long elapsed, RTC_DS3231* rtc, std::vector<unsigned long> lapTimes) {

  String json = "{";
  json += "\"running\":" + String(running);
  json += ",\"sequence\":" + String(sequence);
  json += ",\"remaining\":" + String(remaining);
  json += ",\"elapsed\":" + String(elapsed);
  
  // RTC time and status
  if (rtc != nullptr) {
    DateTime now = rtc->now();
    json += ",\"rtcFound\":true";
    json += ",\"rtcTime\":{";
    json += "\"hour\":" + String(now.hour());
    json += ",\"minute\":" + String(now.minute());
    json += ",\"second\":" + String(now.second());
    json += ",\"day\":" + String(now.day());
    json += ",\"month\":" + String(now.month());
    json += ",\"year\":" + String(now.year());
    json += "}";
  } else {
    json += ",\"rtcFound\":false";
  }
  
  // Lap times
  json += ",\"lapTimes\":[";
  for (size_t i = 0; i < lapTimes.size(); i++) {
    if (i > 0) json += ",";
    json += String(lapTimes[i]);
  }
  json += "]";
  
  // Vlaggen op specifieke momenten tijdens sequence
  if (sequence && remaining > 0) {
    if (remaining > 240000 && remaining <= 300000) {
      // 5:00 - 4:00: Alleen Klassevlag
      json += ",\"flag\":\"class\"";
    } else if (remaining > 60000 && remaining <= 240000) {
      // 4:00 - 1:00: Klassevlag + P-vlag
      json += ",\"flag\":\"class-p\"";
    } else if (remaining > 0 && remaining <= 60000) {
      // 1:00 - 0:00: Alleen Klassevlag (P-vlag weg)
      json += ",\"flag\":\"class\"";
    } else {
      // Voor 5:00: Geen vlag
      json += ",\"flag\":\"none\"";
    }
  } else {
    // Race gestart of gestopt: geen vlag
    json += ",\"flag\":\"none\"";
  }
  
  json += "}";

  Serial.print("[WS] Broadcasting: ");
  Serial.println(json);
  
  ws.broadcastTXT(json);
}

void WebUI::update() {
  ws.loop();
}
