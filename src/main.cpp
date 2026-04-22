#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <LittleFS.h>
#include <Update.h>
#include <time.h>
#include <Wire.h>

#include "config.h"
#include "relay.h"
#include "RaceController.h"
#include "WebUI.h"
#include "version.h"
#include "schedule.h"
#include <RTClib.h>

// ================= CONTROLLER INSTANCES =================
RaceController raceController;
WebUI webUI;
RTC_DS3231 rtc;

// ================= SERVER =================
WebServer server(80);

// ================= BROADCAST THROTTLE =================
unsigned long lastBroadcast = 0;
// ================= CONSTANTS =================
const unsigned long BROADCAST_INTERVAL = 500; // ms - broadcast state every 500ms (2x per second)

// ================= HORN =================
const int HORN = 18;

// ================= FILE UPLOAD =================
File uploadFile;

// ================= HORN =================
void horn(int ms) {
  relaySet(1, 1); // Relay 1 ON
  Serial.println("\n========== HORN ACTIVATED ==========");
  Serial.printf("[HORN] Duration: %d ms\n", ms);
  Serial.printf("[HORN] Pin %d: Setting LOW (active)\n", HORN);
  digitalWrite(HORN, LOW);
  Serial.printf("[HORN] Waiting %d ms...\n", ms);
  delay(ms);
  Serial.printf("[HORN] Pin %d: Setting HIGH (inactive)\n", HORN);
  digitalWrite(HORN, HIGH);
  relaySet(1, 0); // Relay 1 OFF
  Serial.println("[HORN] Horn deactivated");
  Serial.println("[HORN] Relay 1 turned OFF");
  Serial.println("===================================\n");
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
    server.streamFile(f, "text/html; charset=utf-8");
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
  
  // Initialize I2C for RTC (ESP32: SDA=GPIO21, SCL=GPIO22)
  Serial.println("Initializing I2C bus...");
  Wire.begin(21, 22);
  Serial.println("[I2C] Bus initialized (SDA=21, SCL=22)");
  
  // Initialize RTC
  Serial.println("Initializing DS3231 RTC...");
  if (!rtc.begin()) {
    Serial.println("[ERROR] RTC not found on I2C bus!");
    Serial.println("[ERROR] Check I2C connections: SDA=GPIO21, SCL=GPIO22");
  } else {
    Serial.println("[RTC] DS3231 initialized");
    
    if (rtc.lostPower()) {
      Serial.println("[RTC] RTC lost power, setting time from compile time");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    
    DateTime now = rtc.now();
    Serial.printf("[RTC] Current time: %04d-%02d-%02d %02d:%02d:%02d\n",
                  now.year(), now.month(), now.day(),
                  now.hour(), now.minute(), now.second());
  }
  
  Serial.println("Initializing race controller...");
  raceController.begin();
  
  Serial.println("Loading schedule...");
  schedule.begin();
  
  // Configure NTP for time synchronization and sync to RTC
  Serial.println("Configuring NTP...");
  configTime(3600, 3600, "pool.ntp.org", "time.nist.gov");
  
  // Wait for NTP sync and update RTC
  if (cfg.mode == "STA" && WiFi.status() == WL_CONNECTED) {
    Serial.println("[NTP] Waiting for time sync...");
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10000)) {
      Serial.printf("[NTP] Time synced: %04d-%02d-%02d %02d:%02d:%02d\n",
                    timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      
      // Update RTC with NTP time
      rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
      Serial.println("[RTC] Updated from NTP");
    } else {
      Serial.println("[NTP] Time sync failed");
    }
  }
  
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
  
  // Set WebSocket message handler
  webUI.setMessageHandler([](String message) {
    // Trim whitespace from message
    message.trim();
    
    Serial.printf("[WS] Processing message: '%s' (length: %d)\n", message.c_str(), message.length());
    
    if (message == "start") {
      Serial.println("[WS] Starting race sequence");
      startSequence();
    }
    else if (message == "cancel") {
      Serial.println("[WS] Canceling race");
      cancelRace();
    }
    else if (message == "horn") {
      Serial.println("[WS] ✓ Horn command received!");
      horn(2000);  // 2 seconden hoorn
    }
    else {
      Serial.printf("[WS] Unknown command: '%s'\n", message.c_str());
    }
  });
  
  registerConfigRoutes(server);

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
  
  // Trigger signals when step changes (including step 4 for race start)
  if (currentStep != lastStep && currentStep > 0) {
    // Check if sequence is active OR if this is the start signal (step 4)
    bool isSequenceActive = raceController.isSequence();
    bool isRaceStart = (currentStep == 4 && !isSequenceActive && raceController.isRunning());
    
    if (isSequenceActive || isRaceStart) {
      lastStep = currentStep;
      
      // Trigger signals based on step
      switch (currentStep) {
        case 1: // 5 minutes - Warning signal
          Serial.println("[RACE] 5 minuten - Waarschuwing signaal");
          horn(2000); // Horn tone
          activeRelay = 1;
          relayOnTime = millis();
          break;
          
        case 2: // 4 minutes - Preparatory signal  
          Serial.println("[RACE] 4 minuten - Voorbereidend signaal");
          horn(2000);
          activeRelay = 2;
          relayOnTime = millis();
          break;
          
        case 3: // 1 minute - One minute signal
          Serial.println("[RACE] 1 minuut - Een minuut signaal");
          horn(2000);
          activeRelay = 3;
          relayOnTime = millis();
          break;
          
        case 4: // START!
          Serial.println("[RACE] START! Race begint nu!");
          horn(2000); // Horn blast
          activeRelay = 4;
          relayOnTime = millis();
          break;
      }
    }
  }
  
  // Turn off relay after 2 seconds
  if (activeRelay > 0 && (millis() - relayOnTime >= 2000)) {
    relaySet(activeRelay, 0); // Turn off relay
    Serial.printf("[RACE] Relay %d UIT na 2 seconden\n", activeRelay);
    activeRelay = 0;
  }
  
  // Reset when sequence is cancelled (but not if race just started)
  if (!raceController.isSequence() && !raceController.isRunning() && lastStep != -1) {
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
    
    // Get current time from RTC
    DateTime now = rtc.now();
    int scheduleIndex = schedule.checkStartTime(now.hour(), now.minute());
    
    if (scheduleIndex >= 0 && !raceController.isSequence() && !raceController.isRunning()) {
      Serial.printf("[SCHEDULE] Auto-starting for scheduled time: %s\n", 
                    schedule.getTime(scheduleIndex).toString().c_str());
      startSequence();
      schedule.markCompleted(scheduleIndex);
    }
  }

  // Throttled WebSocket broadcast
  unsigned long now = millis();
  if (now - lastBroadcast >= BROADCAST_INTERVAL) {
    lastBroadcast = now;
    webUI.broadcastState(
      raceController.isRunning(),
      raceController.isSequence(),
      raceController.getRemaining(),
      raceController.getElapsed(),
      &rtc
    );
  }
}
