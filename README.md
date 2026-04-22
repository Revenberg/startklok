# ESP32 WiFi Manager - Race Timer

ESP32-based race timing system with web interface, relay control, and LCD display.

## 🚀 Features

- ✅ **Web Dashboard** - Real-time race timer display
- ✅ **WiFi Management** - AP en STA modes met web configuratie
- ✅ **Relay Control** - 4 relay outputs voor start signalen
- ✅ **LCD Display** - 16x2 I2C LCD voor lokale weergave
- ✅ **WebSocket** - Real-time updates (poort 81)
- ✅ **OTA Updates** - Firmware update via web interface
- ✅ **File Upload** - Upload HTML/CSS bestanden naar LittleFS
- ✅ **Version Management** - Automatische versie tracking

## 📋 Hardware

- ESP32 Dev Module
- 4-Channel Relay Module (pins 32, 33, 25, 26)
- 16x2 I2C LCD Display (0x27, SDA=21, SCL=22)
- Horn/Buzzer (pin 18)

## 🔧 Setup

### 1. Install Dependencies

**Arduino Libraries** (via Library Manager):
- WebSockets by Markus Sattler (2.7.2+)
- ArduinoJson by Benoit Blanchon (7.4.3+)
- LiquidCrystal_I2C (1.1.4+)

**Arduino CLI**:
```powershell
arduino-cli core install esp32:esp32
```

### 2. Clone Project

```powershell
git clone https://github.com/Revenberg/startklok.git
cd startklok
```

### 3. Deploy

```powershell
# Auto-detect COM port en deploy
.\deploy.ps1

# Of met specifieke poort
.\deploy.ps1 -Port COM7
```

## 🌐 Web Interface

### First Time Setup

1. ESP32 start in **AP mode** (default SSID: "ESP-RACE")
2. Connect naar WiFi: "ESP-RACE"
3. Open browser: `http://192.168.4.1/setup`
4. Configureer WiFi (STA mode) of blijf in AP mode
5. Reboot ESP32

### Dashboard

- **URL**: `http://[ESP32-IP]/`
- **Features**:
  - Real-time race timer
  - Start/Cancel buttons
  - Live clock
  - Version number (rechtsonder)

### Endpoints

- `GET /` - Dashboard
- `GET /setup` - WiFi configuration
- `GET /status` - JSON status (running, sequence, remaining, version)
- `GET /version` - Version string
- `GET /relay?nr=1&state=1` - Relay control (nr: 1-4, state: 0/1)
- `GET /start` - Start race sequence
- `GET /cancel` - Cancel race
- `POST /upload` - Upload files naar LittleFS
- `POST /update` - OTA firmware update
- `WS ws://[ESP32-IP]:81` - WebSocket (commands: "start", "cancel")

## 🧪 Testing

```powershell
# Quick test
.\test.ps1 -IP 192.168.1.200

# Watch mode (auto-refresh every 5s)
.\test.ps1 -IP 192.168.1.200 -Watch

# Manual curl tests
curl http://192.168.1.200/
curl http://192.168.1.200/status
curl http://192.168.1.200/version
```

## 📦 Deployment

Het deployment script voert automatisch uit:

1. **Version Bump** - Increment build number
2. **Git Check** - Optioneel commit en push
3. **Clean Build** - Verwijder oude build cache
4. **Compile** - Compileer ESP32 firmware
5. **Detect Board** - Zoek ESP32 op COM poorten
6. **Upload** - Flash firmware naar ESP32
7. **Version Commit** - Commit nieuwe versie

**Parameters**:
```powershell
.\deploy.ps1 -Port COM7          # Specifieke COM poort
.\deploy.ps1 -SkipGit            # Skip git operaties
.\deploy.ps1 -Verbose            # Verbose output
.\deploy.ps1 -NoVersionBump      # Geen versie increment
```

## 🔍 Serial Monitor

```powershell
arduino-cli monitor -p COM7 -c baudrate=115200
```

Output toont:
- WiFi connection status
- IP address
- WebSocket events
- Race state changes

## 📁 Project Structure

```
ESP32_WiFi_Manager/
├── ESP32_WiFi_Manager.ino    # Main sketch
├── version.h                  # Version definitions
├── config.cpp/h               # WiFi configuration
├── RaceController.cpp/h       # Race timing logic
├── WebUI.cpp/h                # WebSocket interface
├── lcd.cpp/h                  # LCD display driver
├── relay.cpp/h                # Relay control
├── deploy.ps1                 # Deployment script
├── test.ps1                   # Testing script
├── deploy.md                  # Deployment docs
├── data/                      # Web files (LittleFS)
│   ├── index.html             # Dashboard
│   └── style.css              # Styling
└── upload/                    # Alternative web files
```

## 🎯 Race Controller

**Sequence Timing**:
- T+0s: Sequence start
- T+60s: Warning signal
- T+240s: Preparatory signal
- T+300s: START (sequence → race mode)

**Race Duration**: 5 minutes (300s)

## 🔌 Pin Mapping

| Component | Pin | Note |
|-----------|-----|------|
| Relay 1 | GPIO 32 | Active LOW |
| Relay 2 | GPIO 33 | Active LOW |
| Relay 3 | GPIO 25 | Active LOW |
| Relay 4 | GPIO 26 | Active LOW |
| Horn | GPIO 18 | Active LOW |
| LCD SDA | GPIO 21 | I2C |
| LCD SCL | GPIO 22 | I2C |

## 📝 Version Management

Versie wordt automatisch beheerd in `version.h`:

```cpp
#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 0
#define BUILD_NUMBER 1
#define VERSION_STRING "v1.0.0-b1"
```

Bij elke deploy wordt `BUILD_NUMBER` verhoogd en `VERSION_STRING` geupdate.

## 🐛 Troubleshooting

### Compile Error: "cannot specify '-o'"
```powershell
# Clean build cache
Remove-Item -Path "$env:LOCALAPPDATA\arduino\sketches\*" -Recurse -Force
```

### Upload Error: "Serial port busy"
```powershell
# Stop processen
Get-Process | Where-Object {$_.ProcessName -like "*arduino*"} | Stop-Process -Force
```

### WiFi not connecting
1. Check setup page: `http://192.168.4.1/setup` (in AP mode)
2. Verify SSID/password
3. Reboot ESP32
4. Check serial monitor voor error messages

### Dashboard not loading
```powershell
# Test connectivity
.\test.ps1 -IP [ESP32-IP]

# Check if files uploaded
curl http://[ESP32-IP]/
```

## 📄 License

MIT License - see repository for details

## 🔗 Links

- **GitHub**: https://github.com/Revenberg/startklok
- **ESP32 Arduino Core**: https://github.com/espressif/arduino-esp32
- **WebSockets Library**: https://github.com/Links2004/arduinoWebSockets

## 👥 Contributing

Pull requests welcome! Voor grote wijzigingen, open eerst een issue.

## ✨ Credits

Developed for sailing race timing and start sequence management.
