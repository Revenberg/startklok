#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>

// Bedrading op basis van fysieke pinlabels module (mei 2026):
//   Pin 3: CLK  -> D18 (oranje)
//   Pin 4: MOSI -> D23 (geel)
//   Pin 5: RES  -> D19 (groen)
//   Pin 6: DC   -> D4  (blauw)
//   Pin 7: BLK  -> D15 (paars) -- zetten we HIGH voor backlight
//   Pin 8: MISO -> D2  (grijs)
//   Pin 9: CS1  -> D5  (wit)
// SPI: SCK=18, MOSI=23, MISO=2

#define TFT_CS       5
#define TFT_DC       4
#define TFT_RST     19
#define TFT_BLK     15

#define TOUCH_CS_PIN  13
#define TOUCH_IRQ_PIN 14
#define TOUCH_ENABLED 1

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen touch(TOUCH_CS_PIN, TOUCH_IRQ_PIN);

// Color aliases matching previous test
#define COL_BLACK    ILI9341_BLACK
#define COL_WHITE    ILI9341_WHITE
#define COL_RED      ILI9341_RED
#define COL_GREEN    ILI9341_GREEN
#define COL_BLUE     ILI9341_BLUE
#define COL_YELLOW   ILI9341_YELLOW
#define COL_CYAN     ILI9341_CYAN
#define COL_MAGENTA  ILI9341_MAGENTA
#define COL_NAVY     ILI9341_NAVY
#define COL_ORANGE   ILI9341_ORANGE
#define COL_LGREY    ILI9341_LIGHTGREY
#define COL_DGREY    ILI9341_DARKGREY

unsigned long lastUiUpdateMs = 0;
unsigned long lastTouchPrintMs = 0;
bool lastTouched = false;
bool messageToggle = false;

void drawFigure() {
  // Simple sailboat: hull, mast, two sails, sun
  const int baseX = 92;
  const int baseY = 98;

  tft.fillRoundRect(baseX, baseY + 24, 130, 16, 6, COL_NAVY);
  tft.drawRoundRect(baseX, baseY + 24, 130, 16, 6, COL_WHITE);

  tft.drawLine(baseX + 66, baseY - 18, baseX + 66, baseY + 24, COL_WHITE);
  tft.fillTriangle(baseX + 66, baseY - 18, baseX + 66, baseY + 20, baseX + 104, baseY + 10, COL_ORANGE);
  tft.fillTriangle(baseX + 66, baseY - 10, baseX + 34, baseY + 14, baseX + 66, baseY + 14, COL_CYAN);

  tft.fillCircle(baseX + 120, baseY - 8, 7, COL_YELLOW);
}

void drawMessage(const char* text) {
  tft.fillRect(12, 42, 296, 48, COL_BLACK);
  tft.setTextColor(COL_WHITE, COL_BLACK);
  tft.setTextSize(2);
  tft.setCursor(16, 50);
  tft.println(text);
}

void drawStaticUi() {
  tft.fillScreen(COL_BLACK);

  tft.setTextColor(COL_WHITE, COL_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("TFT 240x320 Test");

  tft.drawRect(8, 36, 304, 60, COL_WHITE);
  drawMessage("Startscherm actief");

  tft.setTextColor(COL_LGREY, COL_BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 96);
  tft.println("Figuurtest: zeilboot + zon");
  drawFigure();

  // Color bars
  tft.fillRect(8,   146, 60, 30, COL_RED);
  tft.fillRect(72,  146, 60, 30, COL_GREEN);
  tft.fillRect(136, 146, 60, 30, COL_BLUE);
  tft.fillRect(200, 146, 60, 30, COL_YELLOW);
  tft.fillRect(264, 146, 48, 30, COL_CYAN);

  tft.drawRect(8, 182, 304, 50, COL_DGREY);
  tft.setTextColor(COL_WHITE, COL_BLACK);
  tft.setTextSize(1);
  tft.setCursor(14, 188);
  tft.println("Raw touch:");
}

void drawTouchData(int x, int y, int z) {
  tft.fillRect(14, 200, 290, 20, COL_BLACK);
  tft.setTextColor(COL_GREEN, COL_BLACK);
  tft.setTextSize(1);
  tft.setCursor(14, 200);
  tft.print("X: "); tft.print(x);
  tft.print("  Y: "); tft.print(y);
  tft.print("  Z: "); tft.print(z);
}

void setup() {
  Serial.begin(115200);
  delay(150);
  Serial.println();
  Serial.println("=== Adafruit ILI9341 Touch Test ===");

  // BLK (pin 7, paars, D15) moet HIGH voor backlight
  pinMode(TFT_BLK, OUTPUT);
  digitalWrite(TFT_BLK, HIGH);

  // MISO zit op D2 (grijs) i.p.v. standaard D19
  SPI.begin(18, 2, 23, TFT_CS);

  tft.begin(4000000); // 4 MHz - laag voor stabiliteit
  tft.setRotation(0);
  tft.invertDisplay(false);

  // Kleurtest: elk 3 seconden met tekst erbij
  tft.fillScreen(ILI9341_RED);
  tft.setTextColor(ILI9341_WHITE); tft.setTextSize(3);
  tft.setCursor(20, 100); tft.println("ROOD");
  delay(3000);

  tft.fillScreen(ILI9341_GREEN);
  tft.setTextColor(ILI9341_BLACK); tft.setTextSize(3);
  tft.setCursor(20, 100); tft.println("GROEN");
  delay(3000);

  tft.fillScreen(ILI9341_BLUE);
  tft.setTextColor(ILI9341_WHITE); tft.setTextSize(3);
  tft.setCursor(20, 100); tft.println("BLAUW");
  delay(3000);

  tft.fillScreen(ILI9341_RED);
  tft.fillRect(20, 60, 200, 120, ILI9341_WHITE);
  tft.setTextColor(ILI9341_BLACK); tft.setTextSize(3);
  tft.setCursor(30, 100); tft.println("WERKT!");
  delay(5000);

  drawStaticUi();
  Serial.println("UI drawn.");

  if (TOUCH_ENABLED) {
    touch.begin();
    touch.setRotation(1);
    Serial.println("Touch init done.");
  } else {
    Serial.println("Touch tijdelijk uit voor TFT-stabiliteitstest.");
  }
}

void loop() {
  // Uptime counter top-right
  if (millis() - lastUiUpdateMs > 800) {
    lastUiUpdateMs = millis();
    tft.setTextColor(COL_ORANGE, COL_BLACK);
    tft.setTextSize(1);
    tft.setCursor(240, 10);
    tft.print(millis() / 1000UL);
    tft.print("s  ");
  }

  if (TOUCH_ENABLED && touch.touched()) {
    TS_Point p = touch.getPoint();
    // Filter phantom reads: real touch has Z between 100 and 3000
    bool realTouch = (p.z > 100 && p.z < 3000);
    if (realTouch) {
      drawTouchData(p.x, p.y, p.z);
      if (!lastTouched) {
        messageToggle = !messageToggle;
        drawMessage(messageToggle ? "Scherm aangeraakt" : "Nogmaals drukken...");
      }
      lastTouched = true;
      if (millis() - lastTouchPrintMs > 120) {
        lastTouchPrintMs = millis();
        Serial.printf("Touch => X:%d Y:%d Z:%d\n", p.x, p.y, p.z);
      }
    } else {
      lastTouched = false;
    }
  } else {
    lastTouched = false;
  }
}
