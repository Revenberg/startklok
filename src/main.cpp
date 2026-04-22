#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <LittleFS.h>
#include <Update.h>

#include "config.h"
#include "relay.h"
#include "RaceController.h"
#include "WebUI.h"
#include "lcd.h"
#include "version.h"

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

  if (LittleFS.exists("/index.html")) {

    File f = LittleFS.open("/index.html", "r");
    server.streamFile(f, "text/html");
    f.close();

  } else {
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

  Serial.println("Initializing hardware...");
  relayInit();
  lcdInit();
  
  Serial.println("Initializing race controller...");
  raceController.begin();

  Serial.println("Loading config...");
  loadConfig();
  
  Serial.println("Starting WiFi...");
  startWiFi();
  
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
    if (type == WStype_TEXT) {
      String msg = String((char*)payload);
      if (msg == "start") startSequence();
      if (msg == "cancel") cancelRace();
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

  raceController.update();

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
