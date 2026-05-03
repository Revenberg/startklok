#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

struct PinSet {
  uint8_t sck;
  uint8_t cs;
  uint8_t dc;
  uint8_t rst;
  uint8_t mosi;
  uint8_t miso;
};

// Candidate combinations: also test alternate SCK routes.
static const PinSet combos[] = {
  {18, 5, 2, 4, 23, 19},
  {18, 5, 2, 4, 19, 23},
  {18, 15, 2, 4, 23, 19},
  {18, 15, 2, 4, 19, 23},
  {18, 5, 13, 4, 23, 19},
  {18, 5, 13, 4, 19, 23},
  {18, 15, 13, 4, 23, 19},
  {18, 15, 13, 4, 19, 23},
  {23, 5, 2, 4, 19, 18},
  {23, 15, 2, 4, 19, 18},
  {23, 5, 13, 4, 19, 18},
  {23, 15, 13, 4, 19, 18},
  {19, 5, 2, 4, 23, 18},
  {19, 15, 2, 4, 23, 18},
  {19, 5, 13, 4, 23, 18},
  {19, 15, 13, 4, 23, 18}
};

static const size_t comboCount = sizeof(combos) / sizeof(combos[0]);
size_t currentCombo = 0;
unsigned long lastSwitchMs = 0;
unsigned long lastTickMs = 0;
const unsigned long holdMs = 7000;

void drawPattern(Adafruit_ILI9341 &tft, const PinSet &p, size_t index) {
  tft.setRotation(0);
  tft.invertDisplay(false);

  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(0, 0, 320, 40, ILI9341_BLUE);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLUE);
  tft.setTextSize(2);
  tft.setCursor(8, 10);
  tft.print("TEST ");
  tft.print(index + 1);

  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setCursor(8, 56);
  tft.print("SCK="); tft.print(p.sck);
  tft.setCursor(8, 82);
  tft.print("CS="); tft.print(p.cs);
  tft.setCursor(8, 108);
  tft.print("DC="); tft.print(p.dc);
  tft.setCursor(8, 134);
  tft.print("RST="); tft.print(p.rst);
  tft.setCursor(8, 160);
  tft.print("MOSI="); tft.print(p.mosi);

  tft.fillRect(8, 186, 95, 48, ILI9341_RED);
  tft.fillRect(113, 186, 95, 48, ILI9341_GREEN);
  tft.fillRect(218, 186, 94, 48, ILI9341_BLUE);

  tft.drawRect(8, 240, 304, 58, ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(14, 246);
  tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
  tft.println("Vind TEST #");
  tft.setTextSize(1);
  tft.setCursor(14, 274);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.println("Stuur alleen testnummer terug.");
}

void runCombo(size_t index) {
  const PinSet &p = combos[index];

  Serial.println();
  Serial.print("==== TEST ");
  Serial.print(index + 1);
  Serial.print(" / ");
  Serial.print(comboCount);
  Serial.println(" ====");
  Serial.print("SCK="); Serial.print(p.sck);
  Serial.print("  CS="); Serial.print(p.cs);
  Serial.print("  DC="); Serial.print(p.dc);
  Serial.print("  RST="); Serial.print(p.rst);
  Serial.print("  MOSI="); Serial.print(p.mosi);
  Serial.print("  MISO="); Serial.println(p.miso);

  pinMode(p.cs, OUTPUT);
  digitalWrite(p.cs, HIGH);

  SPI.end();
  delay(5);
  SPI.begin(p.sck, p.miso, p.mosi, p.cs);

  Adafruit_ILI9341 tft(p.cs, p.dc, p.rst);
  tft.begin(4000000);
  tft.fillScreen(ILI9341_BLACK);
  tft.fillScreen(ILI9341_RED);
  delay(120);
  tft.fillScreen(ILI9341_GREEN);
  delay(120);
  tft.fillScreen(ILI9341_BLUE);
  delay(120);

  drawPattern(tft, p, index);
  Serial.println("Pattern drawn. Check if this is the first stable non-white image.");
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("=== ILI9341 pin scanner ===");
  Serial.println("Scanning SCK/CS/DC/RST/MOSI combinations.");
  Serial.println("Cycling combos every 7s");

  runCombo(currentCombo);
  lastSwitchMs = millis();
}

void loop() {
  if (millis() - lastSwitchMs >= holdMs) {
    currentCombo = (currentCombo + 1) % comboCount;
    runCombo(currentCombo);
    lastSwitchMs = millis();
    lastTickMs = millis();
  }

  if (millis() - lastTickMs >= 1000) {
    lastTickMs = millis();
    Serial.print("Tick TEST ");
    Serial.print(currentCombo + 1);
    Serial.print(" (SCK=");
    Serial.print(combos[currentCombo].sck);
    Serial.print(", CS=");
    Serial.print(combos[currentCombo].cs);
    Serial.print(", DC=");
    Serial.print(combos[currentCombo].dc);
    Serial.print(", RST=");
    Serial.print(combos[currentCombo].rst);
    Serial.print(", MOSI=");
    Serial.print(combos[currentCombo].mosi);
    Serial.println(")");
  }
}
