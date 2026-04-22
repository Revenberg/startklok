#include "WebUI.h"

void WebUI::begin() {
  ws.begin();
}

void WebUI::broadcastState(bool running, bool sequence, unsigned long remaining) {

  String json = "{";
  json += "\"running\":" + String(running);
  json += ",\"sequence\":" + String(sequence);
  json += ",\"remaining\":" + String(remaining);
  json += "}";

  ws.broadcastTXT(json);
}

void WebUI::update() {
  ws.loop();
}