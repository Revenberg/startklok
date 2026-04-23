#include "config.h"
#include "telegram.h"
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
bool tryConnectToNetwork(String ssid, String pass) {
  Serial.printf("[WiFi] Trying to connect to: %s\n", ssid.c_str());
  
  WiFi.begin(ssid.c_str(), pass.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] ✓ Connected to: %s\n", ssid.c_str());
    Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  }
  
  Serial.println("\n[WiFi] ✗ Connection failed");
  return false;
}

void startWiFi() {
  WiFi.mode(WIFI_STA);
  
  // Try multi-network connection if netwerken.txt exists
  if (LittleFS.exists("/netwerken.txt")) {
    Serial.println("[WiFi] Reading netwerken.txt...");
    
    File f = LittleFS.open("/netwerken.txt", "r");
    if (f) {
      // Scan for available networks
      Serial.println("[WiFi] Scanning for networks...");
      int n = WiFi.scanNetworks();
      Serial.printf("[WiFi] Found %d networks\n", n);
      
      // Read networks from file and try to connect
      while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        
        if (line.length() == 0 || line.startsWith("#")) continue;
        
        // Parse line: "SSID PASSWORD"
        int spacePos = line.indexOf(' ');
        if (spacePos > 0) {
          String ssid = line.substring(0, spacePos);
          String pass = line.substring(spacePos + 1);
          pass.trim();
          
          // Check if this network is available
          for (int i = 0; i < n; i++) {
            if (WiFi.SSID(i) == ssid) {
              Serial.printf("[WiFi] Found configured network: %s\n", ssid.c_str());
              
              if (tryConnectToNetwork(ssid, pass)) {
                f.close();
                return;  // Successfully connected
              }
              break;
            }
          }
        }
      }
      f.close();
      
      Serial.println("[WiFi] No configured networks found, falling back to config.json");
    }
  }
  
  // Fallback to config.json (old method)
  if (cfg.mode == "STA") {
    if (tryConnectToNetwork(cfg.ssid, cfg.pass)) {
      return;
    }
  }

  // Fallback to AP mode
  Serial.println("[WiFi] Starting AP mode...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(cfg.ssid.c_str(), cfg.pass.c_str());
  Serial.printf("[WiFi] AP started: %s\n", cfg.ssid.c_str());
  Serial.printf("[WiFi] IP: %s\n", WiFi.softAPIP().toString().c_str());
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

<hr>

<a href="/files">📁 File Management</a> | <a href="/telegram">📱 Telegram Setup</a> | <a href="/">← dashboard</a>

</body>
</html>
)rawliteral");
}

// ================= TELEGRAM SETUP PAGE =================
static void handleTelegram(WebServer &server) {
  server.send(200, "text/html",
R"rawliteral(
<!DOCTYPE html>
<html>
<head><meta charset="UTF-8"><title>Telegram Setup</title></head>
<body style="font-family:Arial;background:#111;color:white;padding:20px;">

<h2>📱 Telegram Setup</h2>

<form method="POST" action="/telegram/save">

Bot Token:<br>
<input name="token" size="50" placeholder="123456789:AAExampleTokenString"><br><br>

Chat ID:<br>
<input name="chatId" placeholder="987654321"><br><br>

<button>Save Telegram Config</button>
</form>

<hr>

<h3>ℹ️ How to get Bot Token and Chat ID:</h3>
<ol>
<li>Talk to @BotFather on Telegram to create a bot and get the token</li>
<li>Talk to @userinfobot to get your Chat ID</li>
<li>Enter both values above and save</li>
</ol>

<br>
<a href="/setup">← Back to Setup</a>

</body>
</html>
)rawliteral");
}

// ================= FILE MANAGEMENT PAGE =================
static void handleFiles(WebServer &server) {
  server.send(200, "text/html",
R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<title>File Management</title>
<style>
body { font-family:Arial; background:#111; color:white; padding:20px; }
table { width:100%; border-collapse:collapse; margin:20px 0; }
th, td { padding:10px; border:1px solid #333; text-align:left; }
th { background:#222; }
button { padding:8px 16px; margin:2px; cursor:pointer; }
.delete-btn { background:#f44; color:white; border:none; }
.download-btn { background:#4a4; color:white; border:none; }
input[type=file] { padding:10px; }
.upload-btn { background:#44a; color:white; border:none; padding:10px 20px; }
</style>
</head>
<body>

<h2>📁 File Management</h2>

<h3>Upload File</h3>
<form method="POST" action="/files/upload" enctype="multipart/form-data">
<input type="file" name="data">
<button class="upload-btn">Upload File</button>
</form>

<hr>

<h3>Files on ESP32</h3>
<table id="fileTable">
<thead>
<tr>
<th>Filename</th>
<th>Size</th>
<th>Actions</th>
</tr>
</thead>
<tbody id="fileList">
<tr><td colspan="3">Loading...</td></tr>
</tbody>
</table>

<br>
<a href="/setup">← Back to Setup</a> | <a href="/">Dashboard</a>

<script>
function loadFiles() {
  fetch('/files/list')
    .then(r => r.json())
    .then(data => {
      const tbody = document.getElementById('fileList');
      tbody.innerHTML = '';
      
      if (data.files.length === 0) {
        tbody.innerHTML = '<tr><td colspan="3">No files found</td></tr>';
        return;
      }
      
      data.files.forEach(file => {
        const row = document.createElement('tr');
        row.innerHTML = `
          <td>${file.name}</td>
          <td>${file.size} bytes</td>
          <td>
            <button class="download-btn" onclick="downloadFile('${file.name}')">Download</button>
            <button class="delete-btn" onclick="deleteFile('${file.name}')">Delete</button>
          </td>
        `;
        tbody.appendChild(row);
      });
    });
}

function downloadFile(name) {
  window.location = '/files/download?name=' + encodeURIComponent(name);
}

function deleteFile(name) {
  if (confirm('Delete ' + name + '?')) {
    fetch('/files/delete?name=' + encodeURIComponent(name), {method: 'POST'})
      .then(r => r.text())
      .then(msg => {
        alert(msg);
        loadFiles();
      });
  }
}

loadFiles();
</script>

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
  
  // ================= FILE MANAGEMENT ROUTES =================
  
  // Files management page
  server.on("/files", [&server]() {
    handleFiles(server);
  });
  
  // List files as JSON
  server.on("/files/list", [&server]() {
    String json = "{\"files\":[";
    
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    bool first = true;
    
    while (file) {
      if (!first) json += ",";
      first = false;
      
      json += "{\"name\":\"";
      json += String(file.name());
      json += "\",\"size\":";
      json += String(file.size());
      json += "}";
      
      file = root.openNextFile();
    }
    
    json += "]}";
    server.send(200, "application/json", json);
  });
  
  // Download file
  server.on("/files/download", [&server]() {
    String filename = server.arg("name");
    
    if (!LittleFS.exists(filename)) {
      server.send(404, "text/plain", "File not found");
      return;
    }
    
    File f = LittleFS.open(filename, "r");
    server.streamFile(f, "application/octet-stream");
    f.close();
  });
  
  // Delete file
  server.on("/files/delete", HTTP_POST, [&server]() {
    String filename = server.arg("name");
    
    if (LittleFS.remove(filename)) {
      server.send(200, "text/plain", "File deleted");
    } else {
      server.send(500, "text/plain", "Delete failed");
    }
  });
  
  // Upload file
  server.on("/files/upload", HTTP_POST,
    [&server]() {
      server.send(200, "text/html", "<script>alert('File uploaded!'); window.location='/files';</script>");
    },
    [&server]() {
      HTTPUpload& upload = server.upload();
      static File uploadFile;
      
      if (upload.status == UPLOAD_FILE_START) {
        String filename = "/" + upload.filename;
        uploadFile = LittleFS.open(filename, "w");
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
  );
}