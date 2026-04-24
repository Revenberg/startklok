#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
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
#include "telegram.h"
#include <RTClib.h>

// ================= CONTROLLER INSTANCES =================
RaceController raceController;
WebUI webUI;
RTC_DS3231 rtc;

// ================= SERVER =================
WebServer server(80);

// ================= CAPTIVE PORTAL =================
DNSServer dnsServer;
static bool captivePortalActive = false;

// ================= BROADCAST THROTTLE =================
unsigned long lastBroadcast = 0;
// ================= CONSTANTS =================
const unsigned long BROADCAST_INTERVAL = 500; // ms - broadcast state every 500ms (2x per second)

// ================= HORN =================
const int HORN = 18;
unsigned long hornStartTime = 0;
int hornDuration = 0;
bool hornActive = false;
bool hornHoldActive = false;

// Horn pattern sequence (for "end" signal)
struct HornPattern {
  int duration;
  bool on;
};
HornPattern endPattern[] = {
  {1000, true},   // 1 sec ON
  {1000, false},  // 1 sec OFF
  {1000, true},   // 1 sec ON
  {1000, false},  // 1 sec OFF
  {2000, true}    // 2 sec ON
};
int endPatternLength = 5;
int endPatternStep = -1;
unsigned long endPatternStartTime = 0;

// ================= FILE UPLOAD =================
File uploadFile;

// ================= HORN (NON-BLOCKING) =================
void hornStart(int ms) {
  if (!hornActive && !hornHoldActive) {
    hornActive = true;
    hornDuration = ms;
    hornStartTime = millis();
    
    relaySet(1, 1); // Relay 1 ON
    Serial.println("\n========== HORN ACTIVATED ==========");
    Serial.printf("[HORN] Duration: %d ms\n", ms);
    Serial.printf("[HORN] Pin %d: Setting LOW (active)\n", HORN);
    digitalWrite(HORN, LOW);
  }
}

void hornHoldStart() {
  if (hornHoldActive || endPatternStep >= 0) return;

  hornHoldActive = true;
  hornActive = false;
  relaySet(1, 1);
  digitalWrite(HORN, LOW);
  Serial.println("[HORN] Hold mode started");
}

void hornHoldStop() {
  if (!hornHoldActive) return;

  hornHoldActive = false;
  digitalWrite(HORN, HIGH);
  relaySet(1, 0);
  Serial.println("[HORN] Hold mode stopped");
}

void hornUpdate() {
  // Handle end pattern sequence
  if (endPatternStep >= 0) {
    unsigned long elapsed = millis() - endPatternStartTime;
    
    if (elapsed >= endPattern[endPatternStep].duration) {
      // Current step complete, move to next
      endPatternStep++;
      
      if (endPatternStep >= endPatternLength) {
        // Pattern complete
        Serial.println("[HORN] End pattern complete");
        digitalWrite(HORN, HIGH);
        relaySet(1, 0);
        endPatternStep = -1;
      } else {
        // Start next step
        endPatternStartTime = millis();
        if (endPattern[endPatternStep].on) {
          digitalWrite(HORN, LOW);
          relaySet(1, 1);
          Serial.printf("[HORN] End pattern step %d: ON (%dms)\n", 
                       endPatternStep, endPattern[endPatternStep].duration);
        } else {
          digitalWrite(HORN, HIGH);
          relaySet(1, 0);
          Serial.printf("[HORN] End pattern step %d: OFF (%dms)\n", 
                       endPatternStep, endPattern[endPatternStep].duration);
        }
      }
    }
    return;  // Don't process normal horn when pattern is active
  }
  
  // Keep horn on while hold button is pressed
  if (hornHoldActive) {
    return;
  }

  // Handle normal horn
  if (hornActive && (millis() - hornStartTime >= hornDuration)) {
    Serial.printf("[HORN] Pin %d: Setting HIGH (inactive)\n", HORN);
    digitalWrite(HORN, HIGH);
    relaySet(1, 0); // Relay 1 OFF
    Serial.println("[HORN] Horn deactivated");
    Serial.println("[HORN] Relay 1 turned OFF");
    Serial.println("===================================\n");
    hornActive = false;
  }
}

void hornStartEndPattern() {
  if (endPatternStep < 0 && !hornActive) {
    Serial.println("\n========== END SIGNAL PATTERN ==========");
    endPatternStep = 0;
    endPatternStartTime = millis();
    digitalWrite(HORN, LOW);
    relaySet(1, 1);
    Serial.println("[HORN] Starting end pattern: 1s ON, 1s OFF, 1s ON, 1s OFF, 2s ON");
  }
}

// ================= CONTROL WRAPPERS =================
void startSequence() {
  raceController.startSequence();
}

void startShortSequence() {
  raceController.startShortSequence();
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
  String ip = (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA)
              ? WiFi.softAPIP().toString()
              : WiFi.localIP().toString();
  json += ",\"ip\":\"" + ip + "\"";
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
  
  Serial.println("Initializing Telegram...");
  telegram.begin();
  
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
  webUI.setMessageHandler([](uint8_t num, String message) {
    // Trim whitespace from message
    message.trim();
    
    Serial.printf("[WS] Processing message: '%s' (length: %d)\n", message.c_str(), message.length());
    
    if (message == "start") {
      Serial.println("[WS] Starting race sequence");
      startSequence();
      telegram.sendMessage("[WS] Starting race sequence");
    }
    else if (message == "startShort") {
      Serial.println("[WS] Starting SHORT race sequence (3 min)");
      startShortSequence();
      telegram.sendMessage("[WS] Starting SHORT race sequence (3 min)");
    }
    else if (message == "cancel") {
      Serial.println("[WS] Canceling race");
      cancelRace();
      telegram.sendMessage("[RACE] Race canceled");
    }
    else if (message == "end") {
      Serial.println("[WS] ✓ End signal command received!");
      hornStartEndPattern();
    }
    else if (message == "horn") {
      Serial.println("[WS] ✓ Horn command received!");
      
      // If during countdown sequence, restart
      if (raceController.isSequence()) {
        Serial.println("[HORN] During sequence - restarting countdown");
        raceController.cancel();
        telegram.sendMessage("[HORN] During sequence - restarting countdown");
        raceController.startSequence();
      } 
      // If in overtime, record lap time
      else if (raceController.isRunning() && raceController.getElapsed() > 300000) {
        hornStart(2000);  // 2 seconden hoorn
        raceController.addLapTime();
        Serial.printf("[LAP] Lap time recorded: %lu ms\n", raceController.getElapsed());
        telegram.sendMessage("[LAP] Lap time recorded: " + String(raceController.getElapsed() / 1000.0, 2) + " seconds");
        // Send lap time notification
        unsigned long overtimeMs = raceController.getElapsed() - 300000;
        int sec = overtimeMs / 1000;
        int m = sec / 60;
        int s = sec % 60;
        String lapMsg = "⏱️ Lap time: +" + String(m) + ":" + (s < 10 ? "0" : "") + String(s);
        telegram.sendMessage(lapMsg);
      }
      // Otherwise just sound horn
      else {
        hornStart(2000);  // 2 seconden hoorn
        telegram.sendMessage("[HORN] Horn activated for 2 seconds");
      }
    }
    else if (message == "lapHoldStart") {
      if (raceController.isRunning() && raceController.getElapsed() > 300000) {
        Serial.println("[LAP] Hold start received");
        hornHoldStart();
      } else {
        Serial.println("[LAP] Hold start ignored (not in overtime)");
      }
    }
    else if (message == "lapHoldStop") {
      if (hornHoldActive) {
        Serial.println("[LAP] Hold stop received");
        hornHoldStop();
        if (raceController.isRunning() && raceController.getElapsed() > 300000) {
          raceController.addLapTime();
          unsigned long overtimeMs = raceController.getElapsed() - 300000;
          int sec = overtimeMs / 1000;
          int m = sec / 60;
          int s = sec % 60;
          String lapMsg = "⏱️ Lap time: +" + String(m) + ":" + (s < 10 ? "0" : "") + String(s);
          telegram.sendMessage(lapMsg);
          Serial.printf("[LAP] Lap saved on release: +%d:%02d\n", m, s);
        }
      }
    }
    else if (message.startsWith("telegram:")) {
      String userMsg = message.substring(9);
      Serial.printf("[WS] Telegram message request: %s\n", userMsg.c_str());
      
      bool success = telegram.sendMessage(userMsg);
      
      // Send response back to client
      String response = success ? "telegram:ok" : "telegram:error";
      webUI.sendToClient(num, response);
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

  // Start captive portal DNS in AP mode
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    dnsServer.start(53, "*", WiFi.softAPIP());
    captivePortalActive = true;
    Serial.println("[Captive] DNS server started - redirecting all to AP IP");

    // Catch-all: stuur onbekende paden door naar de dashboard
    server.onNotFound([]() {
      server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
      server.send(302, "text/plain", "");
    });
  }

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

  hornUpdate();  // Non-blocking horn timing
  webUI.update();
  server.handleClient();
  if (captivePortalActive) {
    dnsServer.processNextRequest();
  }

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
          hornStart(2000); // Horn tone
          activeRelay = 1;
          relayOnTime = millis();
          break;
          
        case 2: // 4 minutes - Preparatory signal  
          Serial.println("[RACE] 4 minuten - Voorbereidend signaal");
          hornStart(2000);
          activeRelay = 2;
          relayOnTime = millis();
          break;
          
        case 3: // 1 minute - One minute signal
          Serial.println("[RACE] 1 minuut - Een minuut signaal");
          hornStart(2000);
          activeRelay = 3;
          relayOnTime = millis();
          break;
          
        case 4: // START!
          Serial.println("[RACE] START! Race begint nu!");
          hornStart(2000); // Horn blast
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
    hornHoldStop();
    relayReset(); // Turn off all relays
    Serial.println("[RACE] Reeks geannuleerd - relays gereset");
  }

  raceController.update();
  
  // Check scheduled starts on minute change (more robust than millis-based 60s polling)
  static unsigned long lastSchedulePoll = 0;
  static int lastCheckedMinute = -1;
  static int lastCheckedDay = -1;
  if (millis() - lastSchedulePoll >= 5000) {
    lastSchedulePoll = millis();

    // Get current time from RTC
    DateTime now = rtc.now();

    // Reset completion flags once per new day
    if (lastCheckedDay != now.day()) {
      schedule.resetCompleted();
      lastCheckedDay = now.day();
    }

    // Only evaluate start condition once per minute
    if (lastCheckedMinute != now.minute()) {
      lastCheckedMinute = now.minute();
      int scheduleIndex = schedule.checkStartTime(now.hour(), now.minute());

      if (scheduleIndex >= 0 && !raceController.isSequence() && !raceController.isRunning()) {
        Serial.printf("[SCHEDULE] Auto-starting for scheduled time: %s\n",
                      schedule.getTime(scheduleIndex).toString().c_str());
        startSequence();
        schedule.markCompleted(scheduleIndex);
      }
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
      &rtc,
      raceController.getLapTimes()
    );
  }
}
