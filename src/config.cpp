#include "config.h"
#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// ================= GLOBAL =================
Config cfg = {"AP", "ESP-RACE", ""};

// ================= LOAD =================
void loadConfig() {

  if (!LittleFS.exists("/config.json")) return;

  File f = LittleFS.open("/config.json", "r");

  StaticJsonDocument<256> doc;
  deserializeJson(doc, f);

  cfg.mode = doc["mode"] | "AP";
  cfg.ssid = doc["ssid"] | "ESP-RACE";
  cfg.pass = doc["pass"] | "";

  f.close();
}

// ================= SAVE =================
void saveConfig() {

  StaticJsonDocument<256> doc;

  doc["mode"] = cfg.mode;
  doc["ssid"] = cfg.ssid;
  doc["pass"] = cfg.pass;

  File f = LittleFS.open("/config.json", "w");
  serializeJson(doc, f);
  f.close();
}

// ================= WIFI =================
void startWiFi() {

  if (cfg.mode == "STA") {
    Serial.printf("Connecting to WiFi SSID: %s\n", cfg.ssid.c_str());
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());

    int t = 0;
    while (WiFi.status() != WL_CONNECTED && t < 20) {
      delay(500);
      Serial.print(".");
      t++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("STA mode: Connection successful!");
      return;
    }
    
    Serial.println("STA mode: Connection failed! Falling back to AP mode...");
    cfg.mode = "AP";  // Update cfg.mode to reflect actual mode
  }

  Serial.printf("Starting AP mode with SSID: %s\n", cfg.ssid.c_str());
  WiFi.mode(WIFI_AP);
  WiFi.softAP(cfg.ssid.c_str(), cfg.pass.c_str());
}

// ================= SETUP PAGE =================
static void handleSetup(WebServer &server) {

  server.send(200, "text/html",
R"rawliteral(
<!DOCTYPE html>
<html>
<head><meta charset="UTF-8"><title>Setup</title></head>
<body style="font-family:Arial;background:#111;color:white;padding:20px;">

<h2>ESP Setup</h2>

<form method="POST" action="/save">

Mode:
<select name="mode">
<option value="AP">AP</option>
<option value="STA">STA</option>
</select><br><br>

SSID:<br>
<input name="ssid"><br><br>

Password:<br>
<input name="pass" type="password"><br><br>

<button>Save</button>
</form>

<hr>

<h3>Firmware Update (OTA)</h3>
<form method="POST" action="/update" enctype="multipart/form-data">
<input type="file" name="update">
<button>Upload Firmware</button>
</form>

<hr>

<h3>Upload Files</h3>
<form method="POST" action="/upload" enctype="multipart/form-data">
<input type="file" name="data">
<button>Upload File</button>
</form>

<br><br>
<a href="/">← dashboard</a>

</body>
</html>
)rawliteral");
}

// ================= REGISTER ROUTES =================
void registerConfigRoutes(WebServer &server) {

  // setup page
  server.on("/setup", [&server]() {
    handleSetup(server);
  });

  // save config
  server.on("/save", HTTP_POST, [&server]() {

    cfg.mode = server.arg("mode");
    cfg.ssid = server.arg("ssid");
    cfg.pass = server.arg("pass");

    saveConfig();

    server.send(200, "text/plain", "Saved - reboot ESP");
  });
}
