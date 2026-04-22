#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <LittleFS.h>
#include <Update.h>
#include <time.h>

#include "config.h"
#include "relay.h"
#include "RaceController.h"
#include "WebUI.h"
#include "lcd.h"
#include "version.h"
#include "schedule.h"

// ================= CONTROLLER INSTANCES =================
RaceController raceController;
WebUI webUI;

// ================= SERVER =================
WebServer server(80);

// ================= BROADCAST THROTTLE =================
unsigned long lastBroadcast = 0;
const unsigned long BROADCAST_INTERVAL = 100; // ms

// ================= HORN =================
const int HORN = 18;

// ================= FILE UPLOAD =================
File uploadFile;

// ================= HORN =================
void horn(int ms) {
  digitalWrite(HORN, LOW);
  delay(ms);
  digitalWrite(HORN, HIGH);
}

// ================= CONTROL WRAPPERS =================
void startSequence() {
  raceController.startSequence();
}

void cancelRace() {
  raceController.cancel();
  relayReset();
}

// ================= ROOT =================
void handleRoot() {
  Serial.println("[HTTP] GET / - Serving index.html");
  
  if (LittleFS.exists("/index.html")) {
    File f = LittleFS.open("/index.html", "r");
    Serial.printf("[HTTP] File size: %d bytes\n", f.size());
    server.streamFile(f, "text/html");
    f.close();
    Serial.println("[HTTP] index.html sent successfully");
  } else {
    Serial.println("[HTTP] ERROR: index.html missing!");
    server.send(404, "text/plain", "index.html missing");
  }
}

// ================= STATUS =================
void handleStatus() {
  String json = "{";
  json += "\"running\":" + String(raceController.isRunning());
  json += ",\"sequence\":" + String(raceController.isSequence());
  json += ",\"remaining\":" + String(raceController.getRemaining());
  json += ",\"version\":\"" + String(VERSION_STRING) + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

// ================= VERSION =================
void handleVersion() {
  server.send(200, "text/plain", VERSION_STRING);
}

// ================= RELAY =================
void handleRelay() {

  int nr = server.arg("nr").toInt();
  int state = server.arg("state").toInt();

  relaySet(nr, state);

  server.send(200, "text/plain", "OK");
}

// ================= FILE UPLOAD =================
void handleFileUpload() {

  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {

    String path = "/" + upload.filename;
    uploadFile = LittleFS.open(path, "w");
  }

  else if (upload.status == UPLOAD_FILE_WRITE) {

    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
    }
  }

  else if (upload.status == UPLOAD_FILE_END) {

    if (uploadFile) uploadFile.close();
  }
}

// ================= OTA =================
void handleOTAUpload() {

  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {

    Update.begin(UPDATE_SIZE_UNKNOWN);
  }

  else if (upload.status == UPLOAD_FILE_WRITE) {

    Update.write(upload.buf, upload.currentSize);
  }

  else if (upload.status == UPLOAD_FILE_END) {

    Update.end(true);
  }
}

// ================= SETUP =================
void setup() {

  Serial.begin(115200);
  delay(100);
  Serial.println("\n\n=== ESP32 Race Controller Starting ===");

  pinMode(HORN, OUTPUT);
  digitalWrite(HORN, HIGH);

  Serial.println("Initializing LittleFS...");
  LittleFS.begin(true);

  Serial.println("Loading WiFi configuration...");
  loadConfig();
  
  Serial.println("Starting WiFi...");
  startWiFi();

  Serial.println("Initializing hardware...");
  relayInit();
  lcdInit();
  
  Serial.println("Initializing race controller...");
  raceController.begin();
  
  Serial.println("Loading schedule...");
  schedule.begin();
  
  // Configure NTP for time synchronization (GMT+1 = 3600, DST = 3600)
  Serial.println("Configuring NTP...");
  configTime(3600, 3600, "pool.ntp.org", "time.nist.gov");
  
  // Print IP address
  if (cfg.mode == "STA" && WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected! IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.print("AP Mode started. IP: ");
    Serial.println(WiFi.softAPIP());
  }
  
  Serial.println("Starting WebSocket server...");
  webUI.begin();
  
  registerConfigRoutes(server);
  
  lcdShowIdle(cfg.mode, cfg.ssid);

  // ================= ROUTES =================
  server.on("/", handleRoot);

  server.on("/status", handleStatus);
  
  server.on("/version", handleVersion);

  // ================= SCHEDULE =================
  server.on("/schedule", HTTP_GET, []() {
    server.send(200, "application/json", schedule.toJson());
  });
  
  server.on("/schedule", HTTP_POST, []() {
    if (!server.hasArg("time")) {
      server.send(400, "text/plain", "Missing time parameter");
      return;
    }
    
    String timeStr = server.arg("time");
    int colonPos = timeStr.indexOf(':');
    if (colonPos <= 0) {
      server.send(400, "text/plain", "Invalid time format");
      return;
    }
    
    int hour = timeStr.substring(0, colonPos).toInt();
    int minute = timeStr.substring(colonPos + 1).toInt();
    
    if (schedule.addTime(hour, minute)) {
      server.send(200, "application/json", schedule.toJson());
    } else {
      server.send(400, "text/plain", "Failed to add time");
    }
  });
  
  server.on("/schedule", HTTP_DELETE, []() {
    if (!server.hasArg("index")) {
      server.send(400, "text/plain", "Missing index parameter");
      return;
    }
    
    int index = server.arg("index").toInt();
    if (schedule.removeTime(index)) {
      server.send(200, "application/json", schedule.toJson());
    } else {
      server.send(400, "text/plain", "Failed to remove time");
    }
  });

  server.on("/relay", handleRelay);

  server.on("/start", []() {
    startSequence();
    server.send(200, "text/plain", "OK");
  });

  server.on("/cancel", []() {
    cancelRace();
    server.send(200, "text/plain", "OK");
  });

  // ================= UPLOAD =================
  server.on("/upload", HTTP_POST,
    []() {
      server.sendHeader("Location", "/setup");
      server.send(303);
    },
    handleFileUpload
  );

  // ================= OTA =================
  server.on("/update", HTTP_POST,
    []() {
      server.send(200, "text/plain", "OK - rebooting");
      delay(500);
      ESP.restart();
    },
    handleOTAUpload
  );

  // ================= WEBSOCKET EVENT HANDLER =================
  webUI.ws.onEvent([](uint8_t num, WStype_t type, uint8_t * payload, size_t len) {
    if (type == WStype_CONNECTED) {
      Serial.printf("[WS] Client #%u connected\n", num);
    }
    else if (type == WStype_DISCONNECTED) {
      Serial.printf("[WS] Client #%u disconnected\n", num);
    }
    else if (type == WStype_TEXT) {
      String msg = String((char*)payload);
      Serial.printf("[WS] Received: %s\n", msg.c_str());
      
      if (msg == "start") {
        Serial.println("[WS] Starting race sequence...");
        startSequence();
      }
      else if (msg == "cancel") {
        Serial.println("[WS] Canceling race...");
        cancelRace();
      }
    }
  });

  Serial.println("Starting web server on port 80...");
  server.begin();
  
  Serial.println("=== READY ===");
  Serial.print("Dashboard: http://");
  if (cfg.mode == "STA" && WiFi.status() == WL_CONNECTED) {
    Serial.print(WiFi.localIP());
  } else {
    Serial.print(WiFi.softAPIP());
  }
  Serial.println("/");
}

// ================= LOOP =================
void loop() {

  webUI.update();
  server.handleClient();

  // Track sequence step changes and relay timing
  static int lastStep = -1;
  static unsigned long relayOnTime = 0;
  static int activeRelay = 0;
  
  int currentStep = raceController.getStep();
  
  // Trigger signals when step changes
  if (raceController.isSequence() && currentStep != lastStep) {
    lastStep = currentStep;
    
    // Trigger signals based on step
    switch (currentStep) {
      case 1: // 5 minutes - Warning signal
        Serial.println("[RACE] 5 minuten - Waarschuwing signaal");
        relaySet(1, 1); // Relay 1 ON
        horn(500); // Horn tone
        activeRelay = 1;
        relayOnTime = millis();
        break;
        
      case 2: // 4 minutes - Preparatory signal  
        Serial.println("[RACE] 4 minuten - Voorbereidend signaal");
        relaySet(2, 1); // Relay 2 ON
        horn(500);
        activeRelay = 2;
        relayOnTime = millis();
        break;
        
      case 3: // 1 minute - One minute signal
        Serial.println("[RACE] 1 minuut - Een minuut signaal");
        relaySet(3, 1); // Relay 3 ON
        horn(500);
        activeRelay = 3;
        relayOnTime = millis();
        break;
        
      case 4: // START!
        Serial.println("[RACE] START!");
        relaySet(4, 1); // Relay 4 ON
        horn(1000); // Long horn blast
        activeRelay = 4;
        relayOnTime = millis();
        break;
    }
  }
  
  // Turn off relay after 2 seconds
  if (activeRelay > 0 && (millis() - relayOnTime >= 2000)) {
    relaySet(activeRelay, 0); // Turn off relay
    Serial.printf("[RACE] Relay %d UIT na 2 seconden\n", activeRelay);
    activeRelay = 0;
  }
  
  // Reset when sequence is cancelled
  if (!raceController.isSequence() && lastStep != -1) {
    lastStep = -1;
    activeRelay = 0;
    relayReset(); // Turn off all relays
    Serial.println("[RACE] Reeks geannuleerd - relays gereset");
  }

  raceController.update();
  
  // Check scheduled starts (every minute)
  static unsigned long lastScheduleCheck = 0;
  if (millis() - lastScheduleCheck >= 60000) { // Check every minute
    lastScheduleCheck = millis();
    
    // Get current time (you need to set time via NTP or manually)
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      int scheduleIndex = schedule.checkStartTime(timeinfo.tm_hour, timeinfo.tm_min);
      
      if (scheduleIndex >= 0 && !raceController.isSequence() && !raceController.isRunning()) {
        Serial.printf("[SCHEDULE] Auto-starting for scheduled time: %s\n", 
                      schedule.getTime(scheduleIndex).toString().c_str());
        startSequence();
        schedule.markCompleted(scheduleIndex);
      }
    }
  }

  // Update LCD display
  if (raceController.isRunning() || raceController.isSequence()) {
    lcdShowRace(raceController.getRemaining());
  } else {
    lcdShowIdle(cfg.mode, cfg.ssid);
  }

  // Throttled WebSocket broadcast
  unsigned long now = millis();
  if (now - lastBroadcast >= BROADCAST_INTERVAL) {
    lastBroadcast = now;
    webUI.broadcastState(
      raceController.isRunning(),
      raceController.isSequence(),
      raceController.getRemaining()
    );
  }
}
