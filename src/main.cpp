#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <LittleFS.h>
#include <Update.h>
#include <time.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>

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
const int HORN = -1;
unsigned long hornStartTime = 0;
int hornDuration = 0;
bool hornActive = false;
bool hornHoldActive = false;

// ================= TFT + TOUCH =================
#define TFT_CS        5
#define TFT_DC        4
#define TFT_RST      19
#define TFT_BLK      15
#define TFT_SCK      18
#define TFT_MOSI     23
#define TFT_MISO      2
#define TOUCH_CS_PIN 13
#define TOUCH_IRQ_PIN 14

// Touch calibration flags for this panel orientation
const bool TOUCH_MIRROR_X = false;
const bool TOUCH_MIRROR_Y = false;

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen touch(TOUCH_CS_PIN, TOUCH_IRQ_PIN);

struct TftButton {
  int x, y, w, h;
  const char* label;
  uint16_t color;
  bool visible;
};

enum TftButtonId {
  BTN_START,        // START 5M - idle only
  BTN_START_SHORT,  // START 3M - idle only
  BTN_STOP,         // STOP     - sequence/running only (full width row 1)
  BTN_END,          // EINDE    - idle only, next to TOETER
  BTN_HORN,         // TOETER   - always
  BTN_R1, BTN_R2, BTN_R3, BTN_R4,
  BTN_COUNT
};

// Layout constants
#define BTN_ROW1_Y  48
#define BTN_ROW2_Y  96
#define BTN_ROW3_Y  144
#define BTN_H       44
#define BTN_H3      38

TftButton tftButtons[BTN_COUNT] = {
  {5,   BTN_ROW1_Y, 150, BTN_H,  "START 5M", ILI9341_DARKGREEN, true},
  {160, BTN_ROW1_Y, 155, BTN_H,  "START 3M", ILI9341_NAVY,      true},
  {5,   BTN_ROW1_Y, 310, BTN_H,  "STOP",     ILI9341_RED,       false},
  {160, BTN_ROW2_Y, 155, BTN_H,  "EINDE",    ILI9341_ORANGE,    false},
  {5,   BTN_ROW2_Y, 150, BTN_H,  "TOETER",   ILI9341_MAGENTA,   true},
  {5,   BTN_ROW3_Y,  73, BTN_H3, "R1",       ILI9341_NAVY,      true},
  {83,  BTN_ROW3_Y,  73, BTN_H3, "R2",       ILI9341_NAVY,      true},
  {161, BTN_ROW3_Y,  73, BTN_H3, "R3",       ILI9341_NAVY,      true},
  {239, BTN_ROW3_Y,  76, BTN_H3, "R4",       ILI9341_NAVY,      true}
};

String tftLastAction = "Klaar";
bool tftTouchDown = false;
int tftTouchButtonCandidate = -1;
unsigned long tftTouchPressStartMs = 0;
unsigned long tftLastRefresh = 0;
int tftLastRemainingSec = -1;
bool tftLastSequence = false;
bool tftLastRunning = false;
int tftLastRelayState[4] = {-1, -1, -1, -1};
char tftLastTimeStr[9] = "";
char tftLastNextStart[6] = "";
int tftLastDeviceCount = -1;

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
    // Start-priority: switch outputs immediately on trigger.
    relaySet(1, 1); // Relay 1 ON
    if (HORN >= 0) {
      digitalWrite(HORN, LOW);
    }

    hornActive = true;
    hornDuration = ms;
    hornStartTime = millis();

    Serial.println("\n========== HORN ACTIVATED ==========");
    Serial.printf("[HORN] Duration: %d ms\n", ms);
    Serial.printf("[HORN] Pin %d: Setting LOW (active)\n", HORN);
  }
}

void hornHoldStart() {
  if (hornHoldActive || endPatternStep >= 0) return;

  hornHoldActive = true;
  hornActive = false;
  relaySet(1, 1);
  if (HORN >= 0) {
    digitalWrite(HORN, LOW);
  }
  Serial.println("[HORN] Hold mode started");
}

void hornHoldStop() {
  if (!hornHoldActive) return;

  hornHoldActive = false;
  if (HORN >= 0) {
    digitalWrite(HORN, HIGH);
  }
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
        if (HORN >= 0) {
          digitalWrite(HORN, HIGH);
        }
        relaySet(1, 0);
        endPatternStep = -1;
      } else {
        // Start next step
        endPatternStartTime = millis();
        if (endPattern[endPatternStep].on) {
          if (HORN >= 0) {
            digitalWrite(HORN, LOW);
          }
          relaySet(1, 1);
          Serial.printf("[HORN] End pattern step %d: ON (%dms)\n", 
                       endPatternStep, endPattern[endPatternStep].duration);
        } else {
          if (HORN >= 0) {
            digitalWrite(HORN, HIGH);
          }
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
    if (HORN >= 0) {
      digitalWrite(HORN, HIGH);
    }
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
    if (HORN >= 0) {
      digitalWrite(HORN, LOW);
    }
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

bool tftContains(const TftButton& b, int x, int y) {
  if (!b.visible) return false;
  return x >= b.x && x < (b.x + b.w) && y >= b.y && y < (b.y + b.h);
}

void tftDrawButton(const TftButton& b, bool active) {
  uint16_t bg = active ? ILI9341_GREEN : b.color;
  tft.fillRect(b.x, b.y, b.w, b.h, bg);
  tft.drawRect(b.x, b.y, b.w, b.h, ILI9341_WHITE);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  // Center label in button
  int16_t tx = b.x + (b.w - (int)strlen(b.label) * 12) / 2;
  int16_t ty = b.y + (b.h / 2) - 8;
  tft.setCursor(tx, ty);
  tft.print(b.label);
}

// Get next non-completed scheduled start as "HH:MM" or "--:--"
void tftGetNextStart(char* buf, int bufSize) {
  if (schedule.getCount() == 0) { strncpy(buf, "--:--", bufSize); return; }
  DateTime now = rtc.now();
  int nowMin = now.hour() * 60 + now.minute();
  int bestMin = -1, bestH = -1, bestM = -1;
  for (int i = 0; i < schedule.getCount(); i++) {
    ScheduleTime t = schedule.getTime(i);
    if (t.completed) continue;
    int tMin = t.toMinutes();
    if (tMin > nowMin && (bestMin < 0 || tMin < bestMin)) {
      bestMin = tMin; bestH = t.hour; bestM = t.minute;
    }
  }
  if (bestMin < 0) strncpy(buf, "--:--", bufSize);
  else snprintf(buf, bufSize, "%02d:%02d", bestH, bestM);
}

// Update button visibility based on race state and redraw changed buttons
void tftUpdateContextButtons(bool force, bool running, bool sequence) {
  bool active = running || sequence;
  bool wasActive = tftLastRunning || tftLastSequence;
  if (!force && active == wasActive) return;

  // Clear the two action button rows
  tft.fillRect(0, BTN_ROW1_Y, 320, BTN_H + BTN_H + 4, ILI9341_BLACK);

  if (active) {
    // Active: STOP (full width row 1)
    tftButtons[BTN_START].visible       = false;
    tftButtons[BTN_START_SHORT].visible = false;
    tftButtons[BTN_STOP].visible        = true;
    tftButtons[BTN_END].visible         = false;
    tftButtons[BTN_HORN].visible        = true;
  } else {
    // Idle: START 5M + START 3M in row 1
    tftButtons[BTN_START].visible       = true;
    tftButtons[BTN_START_SHORT].visible = true;
    tftButtons[BTN_STOP].visible        = false;
    tftButtons[BTN_END].visible         = true;
    tftButtons[BTN_HORN].visible        = true;
  }

  // Draw visible action buttons (not relays)
  for (int i = 0; i <= BTN_HORN; i++) {
    if (tftButtons[i].visible) tftDrawButton(tftButtons[i], false);
  }
}

void tftDrawStaticUi() {
  tft.fillScreen(ILI9341_BLACK);
  // Draw relay buttons (always visible)
  for (int i = BTN_R1; i <= BTN_R4; i++) {
    tftDrawButton(tftButtons[i], relayGet((i - BTN_R1) + 1) != 0);
  }
  // Bottom action bar border
  tft.drawFastHLine(0, 190, 320, ILI9341_DARKCYAN);
}

void tftDrawDynamicUi(bool force) {
  bool running  = raceController.isRunning();
  bool sequence = raceController.isSequence();
  int  remainingSec = (int)(raceController.getRemaining() / 1000UL);

  // --- Context buttons (only redraw when state changes) ---
  tftUpdateContextButtons(force, running, sequence);

  // --- Time row (top left HH:MM:SS) ---
  {
    DateTime now = rtc.now();
    char timeStr[9];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    if (force || strcmp(timeStr, tftLastTimeStr) != 0) {
      strncpy(tftLastTimeStr, timeStr, sizeof(tftLastTimeStr));
      tft.fillRect(0, 2, 130, 18, ILI9341_BLACK);
      tft.setTextSize(2);
      tft.setTextColor(ILI9341_CYAN);
      tft.setCursor(2, 4);
      tft.print(timeStr);
    }
    // Next start (top right)
    char nextStr[6];
    int deviceCount = WiFi.softAPgetStationNum();
    tftGetNextStart(nextStr, sizeof(nextStr));
    if (force || strcmp(nextStr, tftLastNextStart) != 0 || deviceCount != tftLastDeviceCount) {
      strncpy(tftLastNextStart, nextStr, sizeof(tftLastNextStart));
      tftLastDeviceCount = deviceCount;
      tft.fillRect(135, 2, 185, 18, ILI9341_BLACK);
      tft.setTextSize(2);
      tft.setTextColor(ILI9341_YELLOW);
      tft.setCursor(137, 4);
      tft.printf("Vgl:%s D:%d", nextStr, deviceCount);
    }
  }

  // --- Status bar (race timer) ---
  if (force || running != tftLastRunning || sequence != tftLastSequence || remainingSec != tftLastRemainingSec) {
    tft.fillRect(0, 24, 320, 20, ILI9341_BLACK);
    tft.setTextSize(2);
    if (sequence) {
      tft.setTextColor(ILI9341_YELLOW);
      tft.setCursor(2, 26);
      int m = remainingSec / 60, s = remainingSec % 60;
      tft.printf("AFTELLEN  %d:%02d", m, s);
    } else if (running) {
      tft.setTextColor(ILI9341_GREEN);
      tft.setCursor(2, 26);
      unsigned long ov = raceController.getElapsed() > 300000UL ? raceController.getElapsed() - 300000UL : 0;
      int sec = (int)(ov / 1000UL);
      tft.printf("RACE  +%d:%02d", sec / 60, sec % 60);
    } else {
      tft.setTextColor(ILI9341_DARKGREY);
      tft.setCursor(2, 26);
      tft.print("KLAAR");
    }
    tftLastRunning    = running;
    tftLastSequence   = sequence;
    tftLastRemainingSec = remainingSec;
  }

  // --- Relay buttons ---
  for (int i = 0; i < 4; i++) {
    int state = relayGet(i + 1);
    if (force || state != tftLastRelayState[i]) {
      tftLastRelayState[i] = state;
      tftDrawButton(tftButtons[BTN_R1 + i], state != 0);
    }
  }

  // --- Last action text ---
  {
    static String lastDrawnAction = "";
    if (force || tftLastAction != lastDrawnAction) {
      lastDrawnAction = tftLastAction;
      tft.fillRect(0, 193, 320, 20, ILI9341_BLACK);
      tft.setTextSize(2);
      tft.setTextColor(ILI9341_WHITE);
      tft.setCursor(4, 195);
      tft.print(tftLastAction);
    }
  }
}

bool tftReadTouch(int& sx, int& sy) {
  if (!touch.touched()) {
    return false;
  }

  TS_Point p = touch.getPoint();
  if (p.z < 100 || p.z > 3500) {
    return false;
  }

  // touch.setRotation(1) already transforms coordinates.
  // Apply optional mirroring to align touch with drawn UI.
  const int rawMin = 200;
  const int rawMax = 3900;

  sx = map(constrain(p.x, rawMin, rawMax), rawMin, rawMax, 0, 319);
  sy = map(constrain(p.y, rawMin, rawMax), rawMin, rawMax, 239, 0);

  if (TOUCH_MIRROR_X) sx = 319 - sx;
  if (TOUCH_MIRROR_Y) sy = 239 - sy;

  return true;
}

void tftHandleButtonPress(TftButtonId id) {
  switch (id) {
    case BTN_START:
      startSequence();
      tftLastAction = "Start 5 min";
      break;
    case BTN_START_SHORT:
      startShortSequence();
      tftLastAction = "Start 3 min";
      break;
    case BTN_STOP:
      cancelRace();
      tftLastAction = "Gestopt";
      break;
    case BTN_END:
      hornStartEndPattern();
      tftLastAction = "Einde signaal";
      break;
    case BTN_HORN:
      hornStart(2000);
      tftLastAction = "Toeter 2 sec";
      break;
    case BTN_R1:
    case BTN_R2:
    case BTN_R3:
    case BTN_R4: {
      int relayNr = (id - BTN_R1) + 1;
      int newState = relayGet(relayNr) ? 0 : 1;
      relaySet(relayNr, newState);
      tftLastAction = "Relay " + String(relayNr) + (newState ? " AAN" : " UIT");
      break;
    }
    default:
      break;
  }
}

void tftHandleTouch() {
  int tx = 0;
  int ty = 0;
  bool touched = tftReadTouch(tx, ty);

  if (touched) {
    if (!tftTouchDown) {
      tftTouchDown = true;
      tftTouchPressStartMs = millis();
      tftTouchButtonCandidate = -1;
      for (int i = 0; i < BTN_COUNT; i++) {
        if (tftContains(tftButtons[i], tx, ty)) {
          tftTouchButtonCandidate = i;
          break;
        }
      }
      if (tftTouchButtonCandidate < 0) {
        Serial.printf("[TOUCH] Geen knop op sx=%d sy=%d\n", tx, ty);
      }
    }
  } else if (tftTouchDown) {
    unsigned long pressMs = millis() - tftTouchPressStartMs;
    if (tftTouchButtonCandidate >= 0) {
      if (pressMs >= 50 && pressMs <= 1000) {
        tftHandleButtonPress((TftButtonId)tftTouchButtonCandidate);
        // Force full redraw after accepted button press (state may have changed)
        tftDrawDynamicUi(true);
      } else {
        Serial.printf("[TOUCH] Knop genegeerd, duur=%lums (toegestaan: 50-1000ms)\n", pressMs);
      }
    }

    tftTouchDown = false;
    tftTouchButtonCandidate = -1;
  }
}

void tftHardwareInit() {
  pinMode(TFT_BLK, OUTPUT);
  digitalWrite(TFT_BLK, HIGH);

  SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI, TFT_CS);
  tft.begin(4000000);
  tft.setRotation(1);

  touch.begin();
  touch.setRotation(1);

  // Solid black screen so we know display works
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_CYAN);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.print("Opstarten...");
}

void tftDrawInitialUi() {
  tftDrawStaticUi();
  tftDrawDynamicUi(true);
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

void handleStyle() {
  if (LittleFS.exists("/style.css")) {
    File f = LittleFS.open("/style.css", "r");
    server.streamFile(f, "text/css; charset=utf-8");
    f.close();
  } else {
    server.send(404, "text/plain", "style.css missing");
  }
}

void handleAppJs() {
  if (LittleFS.exists("/app.js")) {
    File f = LittleFS.open("/app.js", "r");
    server.streamFile(f, "application/javascript; charset=utf-8");
    f.close();
  } else {
    server.send(404, "text/plain", "app.js missing");
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

  if (HORN >= 0) {
    pinMode(HORN, OUTPUT);
    digitalWrite(HORN, HIGH);
  }

  Serial.println("Initializing LittleFS...");
  LittleFS.begin(true);

  Serial.println("Loading WiFi configuration...");
  loadConfig();
  
  Serial.println("Starting WiFi...");
  startWiFi();

  Serial.println("Initializing hardware...");
  relayInit();

  // TFT hardware FIRST - before Wire/I2C so SPI bus is undisturbed
  Serial.println("Initializing TFT hardware...");
  tftHardwareInit();

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

  // Draw full UI now that RTC and schedule are ready
  Serial.println("Drawing TFT UI...");
  tftDrawInitialUi();
  
  Serial.println("Initializing Telegram...");
  telegram.begin();
  
  // Configure NTP for time synchronization and sync to RTC
  Serial.println("Configuring NTP...");
  configTzTime("CET-1CEST,M3.5.0/2,M10.5.0/3", "pool.ntp.org", "time.nist.gov");
  
  // Wait for NTP sync and update RTC
  bool wifiConnected = WiFi.status() == WL_CONNECTED;
  if (wifiConnected) {
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
  
  // Print actual network status instead of configured mode.
  if (wifiConnected) {
    Serial.print("WiFi connected! IP: ");
    Serial.println(WiFi.localIP());
  } else if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    Serial.print("AP Mode started. IP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("[WiFi] Not connected and AP not active");
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

      // Start-priority mode: trigger horn first, handle all extra logic afterwards.
      hornStart(2000);  // 2 seconden hoorn
      
      // If during countdown sequence, restart
      if (raceController.isSequence()) {
        Serial.println("[HORN] During sequence - restarting countdown");
        raceController.cancel();
        telegram.sendMessage("[HORN] During sequence - restarting countdown");
        raceController.startSequence();
      } 
      // If in overtime, record lap time
      else if (raceController.isRunning() && raceController.getElapsed() > 300000) {
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
  server.on("/style.css", handleStyle);
  server.on("/app.js", handleAppJs);

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

    auto captiveRedirect = []() {
      server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
      server.send(302, "text/plain", "");
    };

    // Captive portal probes used by Android, iOS/macOS, and Windows
    server.on("/generate_204", captiveRedirect);      // Android
    server.on("/gen_204", captiveRedirect);           // Android (fallback)
    server.on("/hotspot-detect.html", captiveRedirect); // Apple
    server.on("/ncsi.txt", captiveRedirect);          // Windows NCSI
    server.on("/connecttest.txt", captiveRedirect);   // Windows connect test
    server.on("/redirect", captiveRedirect);          // Generic captive probe
    server.on("/fwlink", captiveRedirect);            // Microsoft fwlink probe

    // Catch-all: stuur onbekende paden door naar de dashboard
    server.onNotFound(captiveRedirect);
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
  tftHandleTouch();
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
  if (now - tftLastRefresh >= 200) {
    tftLastRefresh = now;
    tftDrawDynamicUi(false);
  }
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
