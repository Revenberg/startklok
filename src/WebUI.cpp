#include "WebUI.h"

void WebUI::begin() {
  ws.begin();
}

void WebUI::broadcastState(bool running, bool sequence, unsigned long remaining) {

  String json = "{";
  json += "\"running\":" + String(running);
  json += ",\"sequence\":" + String(sequence);
  json += ",\"remaining\":" + String(remaining);
  
  // Determine flag phase during sequence
  if (sequence && remaining > 0) {
    if (remaining > 240000) {
      json += ",\"flag\":\"class\"";  // 5:00 - 4:00: Klassevlag
    } else if (remaining > 60000) {
      json += ",\"flag\":\"p\"";      // 4:00 - 1:00: P-vlag
    } else {
      json += ",\"flag\":\"p-down\""; // 1:00 - 0:00: P-vlag down
    }
  } else if (running) {
    json += ",\"flag\":\"racing\"";   // Race started, class flag down
  } else {
    json += ",\"flag\":\"none\"";     // No flag
  }
  
  json += "}";

  ws.broadcastTXT(json);
}

void WebUI::update() {
  ws.loop();
}
