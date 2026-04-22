# ESP32 Deploy Workflow

Geautomatiseerde deployment workflow voor ESP32 WiFi Manager project met automatische versie incrementatie.

## Features

✅ **Automatische versie incrementatie** - Build nummer wordt automatisch verhoogd bij elke deploy  
✅ **Versienummer in web interface** - Versie wordt rechtsonder in het scherm getoond  
✅ **Git integratie** - Optioneel committen en pushen naar GitHub  
✅ **Auto-detect COM poorten** - Detecteert automatisch aangesloten ESP32 boards  

## Vereisten

- Arduino CLI geïnstalleerd
- ESP32 board support geïnstalleerd: `arduino-cli core install esp32:esp32`
- Benodigde libraries:
  - WebSockets (2.7.2+)
  - ArduinoJson
  - LiquidCrystal_I2C

## Deploy Stappen

### 1. Git Push (optioneel)

```powershell
# Check voor wijzigingen
git status

# Als er wijzigingen zijn:
git add .
git commit -m "Beschrijf wijzigingen"
git push
```

### 2. Clean Build Cache (bij compile problemen)

```powershell
# Verwijder Arduino build cache
Remove-Item -Path "$env:LOCALAPPDATA\arduino\sketches\*" -Recurse -Force -ErrorAction SilentlyContinue
```

### 3. Compileer ESP32 Code

```powershell
# Standaard compilatie
arduino-cli compile --fqbn esp32:esp32:esp32 .

# Of met output directory
arduino-cli compile --fqbn esp32:esp32:esp32 --output-dir build .

# Met verbose output (voor debugging)
arduino-cli compile --fqbn esp32:esp32:esp32 --verbose .
```

### 4. Detecteer ESP32 COM Poort

```powershell
# Toon alle aangesloten boards
arduino-cli board list

# Filter alleen ESP32 devices
Get-CimInstance -ClassName Win32_PnPEntity | Where-Object {$_.Name -like "*USB*" -or $_.Name -like "*Serial*"} | Select-Object Name, DeviceID
```

### 5. Upload naar ESP32

```powershell
# Upload naar specifieke poort (vervang COM7 met gevonden poort)
arduino-cli upload -p COM7 --fqbn esp32:esp32:esp32 .

# Met verbose output
arduino-cli upload -p COM7 --fqbn esp32:esp32:esp32 --verbose .
```

## Complete Deploy Script

```powershell
# 1. Git push (optioneel)
git status
# git add . && git commit -m "Update" && git push

# 2. Clean build
Remove-Item -Path "$env:LOCALAPPDATA\arduino\sketches\*" -Recurse -Force -ErrorAction SilentlyContinue

# 3. Compileer
arduino-cli compile --fqbn esp32:esp32:esp32 --output-dir build .

# 4. Detecteer poort
$boards = arduino-cli board list
Write-Host $boards

# 5. Upload (vervang COM7)
arduino-cli upload -p COM7 --fqbn esp32:esp32:esp32 .
```

## Troubleshooting

### Compile Error: "file is being used by another process"

```powershell
# Stop alle Arduino processen
Get-Process | Where-Object {$_.ProcessName -like "*arduino*"} | Stop-Process -Force

# Verwijder build cache
Remove-Item -Path "$env:LOCALAPPDATA\arduino\sketches\*" -Recurse -Force
```

### Compile Error: "cannot specify '-o' with '-c'"

Dit wijst vaak op conflicterende bestanden. Oplossingen:

1. Gebruik `--output-dir build` parameter
2. Hernoem project folder naar exacte .ino bestandsnaam
3. Check voor dubbele .ino bestanden in dezelfde folder

### Upload Error: "Serial port not found"

```powershell
# Check beschikbare COM poorten
Get-CimInstance -ClassName Win32_SerialPort | Select-Object Name, DeviceID

# Of met Mode command
mode
```

### Monitor Serial Output

```powershell
# Monitor serial output (9600 baud = standaard, 115200 voor dit project)
arduino-cli monitor -p COM7 -c baudrate=115200
```

## Debug & Testing

### Test Dashboard (via curl)

**Vervang `192.168.1.200` met het IP adres van je ESP32**

```powershell
# Test dashboard homepage
curl http://192.168.1.200/

# Test status API (JSON response)
curl http://192.168.1.200/status

# Test version endpoint
curl http://192.168.1.200/version

# Test setup page
curl http://192.168.1.200/setup

# Test relay control (relay 1 aan)
curl "http://192.168.1.200/relay?nr=1&state=1"

# Test relay control (relay 1 uit)
curl "http://192.168.1.200/relay?nr=1&state=0"

# Start race sequence
curl http://192.168.1.200/start

# Cancel race
curl http://192.168.1.200/cancel
```

### Test in Browser

Open een browser en navigeer naar:
- **Dashboard**: `http://192.168.1.200/`
- **Setup**: `http://192.168.1.200/setup`
- **Status API**: `http://192.168.1.200/status`
- **Version**: `http://192.168.1.200/version`

### IP Adres Vinden

Via Serial Monitor (115200 baud):
```
Connecting to WiFi...
WiFi connected
IP address: 192.168.1.200
```

Of via Arduino IDE Serial Monitor of arduino-cli:
```powershell
arduino-cli monitor -p COM7 -c baudrate=115200
```

### WebSocket Test

WebSocket server draait op poort **81**:
- Endpoint: `ws://192.168.1.200:81`
- Commands: `"start"`, `"cancel"`

### Quick Test Script

Maak een `test.ps1` voor snelle tests:

```powershell
param([string]$IP = "192.168.1.200")

Write-Host "Testing ESP32 at $IP..." -ForegroundColor Cyan

Write-Host "`n1. Version:" -ForegroundColor Yellow
curl "http://$IP/version"

Write-Host "`n`n2. Status:" -ForegroundColor Yellow
curl "http://$IP/status"

Write-Host "`n`n3. Dashboard available at:" -ForegroundColor Green
Write-Host "http://$IP/" -ForegroundColor White
```

Gebruik: `.\test.ps1 -IP 192.168.1.200`

## Board Specifieke Settings

Voor ESP32 Dev Module:
- FQBN: `esp32:esp32:esp32`
- Upload Speed: 921600
- Flash Frequency: 80MHz
- Flash Mode: QIO
- Flash Size: 4MB
- Partition Scheme: Default

## Automation Script

Voor volledige automatisering, gebruik `deploy.ps1`:

**Parameters:**
- `-Port` - COM poort (bijv. COM7). Auto-detecteert als niet opgegeven
- `-SkipGit` - Skip git operaties
- `-Verbose` - Verbose output tijdens compilatie en upload
- `-NoVersionBump` - Skip automatische versie incrementatie

**Voorbeelden:**

```powershell
# Eenvoudig (auto-detect poort, bump versie)
.\deploy.ps1

# Met specifieke poort
.\deploy.ps1 -Port COM7

# Skip git, met verbose output
.\deploy.ps1 -Port COM7 -SkipGit -Verbose

# Zonder versie bump
.\deploy.ps1 -NoVersionBump
```

**Workflow stappen:**
1. **[0/6]** Increment build number in version.h
2. **[1/6]** Check git status (optioneel push naar GitHub)
3. **[2/6]** Clean build cache
4. **[3/6]** Compile ESP32 code
5. **[4/6]** Detect ESP32 boards op COM poorten
6. **[5/6]** Upload firmware naar ESP32
7. **[6/6]** Commit versie bump naar git

## Versie Management

Het versienummer wordt automatisch beheerd in `version.h`:

```cpp
#define VERSION_MAJOR 1
#define VERSION_MINOR 0  
#define VERSION_PATCH 0
#define BUILD_NUMBER 1
#define VERSION_STRING "v1.0.0-b1"
```

Bij elke deploy:
- BUILD_NUMBER wordt verhoogd
- VERSION_STRING wordt geupdate
- Versie wordt getoond rechtsonder in web interface
- Versie wordt toegevoegd aan `/status` API response

**Handmatig versie aanpassen:**
```powershell
# Edit version.h en pas VERSION_MAJOR/MINOR/PATCH aan
# BUILD_NUMBER wordt automatisch verhoogd bij volgende deploy
```

## Complete Deploy Script
Write-Host "`n[5/5] Uploading to $Port..." -ForegroundColor Yellow
$uploadCmd = "arduino-cli upload -p $Port --fqbn esp32:esp32:esp32 ."
if ($Verbose) { $uploadCmd += " --verbose" }
Invoke-Expression $uploadCmd

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n=== Deploy Successful! ===" -ForegroundColor Green
} else {
    Write-Host "`n=== Deploy Failed! ===" -ForegroundColor Red
    exit 1
}
```

Gebruik: `.\deploy.ps1 -Port COM7 -Verbose`
