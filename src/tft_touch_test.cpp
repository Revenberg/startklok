#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// Test sketch for common 2.4 inch 240x320 SPI TFT + XPT2046 touch modules.
// Adjust these pins to your wiring if needed.
static const uint8_t TOUCH_CS_PIN = 15;
static const uint8_t TOUCH_IRQ_PIN = 4;

TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen touch(TOUCH_CS_PIN, TOUCH_IRQ_PIN);

unsigned long lastUiUpdateMs = 0;
unsigned long lastTouchPrintMs = 0;
bool lastTouched = false;
bool messageToggle = false;

void drawFigure() {
  // Draw a simple sailboat figure to verify lines, triangles and circles.
  const int baseX = 92;
  const int baseY = 98;

  tft.fillRoundRect(baseX, baseY + 24, 130, 16, 6, TFT_NAVY);
  tft.drawRoundRect(baseX, baseY + 24, 130, 16, 6, TFT_WHITE);

  tft.drawLine(baseX + 66, baseY - 18, baseX + 66, baseY + 24, TFT_WHITE);
  tft.fillTriangle(baseX + 66, baseY - 18, baseX + 66, baseY + 20, baseX + 104, baseY + 10, TFT_ORANGE);
  tft.fillTriangle(baseX + 66, baseY - 10, baseX + 34, baseY + 14, baseX + 66, baseY + 14, TFT_CYAN);

  tft.fillCircle(baseX + 120, baseY - 8, 7, TFT_YELLOW);
}

void drawMessage(const char* text) {
  tft.fillRect(12, 42, 296, 48, TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  tft.setCursor(16, 50);
  tft.println(text);
}

void drawStaticUi() {
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextFont(2);
  tft.setCursor(10, 10);
  tft.println("TFT 240x320 Test");

  tft.drawRect(8, 36, 304, 60, TFT_WHITE);
  drawMessage("Startscherm actief");

  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setCursor(10, 96);
  tft.println("Figuurtest: zeilboot + zon");
  drawFigure();

  // Color bars to quickly verify RGB output and orientation.
  tft.fillRect(8, 146, 60, 30, TFT_RED);
  tft.fillRect(72, 146, 60, 30, TFT_GREEN);
  tft.fillRect(136, 146, 60, 30, TFT_BLUE);
  tft.fillRect(200, 146, 60, 30, TFT_YELLOW);
  tft.fillRect(264, 146, 48, 30, TFT_CYAN);

  tft.drawRect(8, 182, 304, 128, TFT_DARKGREY);
  tft.setCursor(14, 188);
  tft.println("Raw touch:");
}

void drawTouchData(int x, int y, int z) {
  tft.fillRect(14, 210, 290, 28, TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(14, 210);
  tft.printf("X: %4d  Y: %4d  Z: %4d", x, y, z);

  // Draw a marker in the touch area (mapped from raw range).
  int px = map(x, 200, 3800, 10, 302);
  int py = map(y, 200, 3800, 184, 307);
  px = constrain(px, 10, 302);
  py = constrain(py, 184, 307);

  tft.fillRect(10, 240, 300, 68, TFT_BLACK);
  tft.drawLine(px - 6, py, px + 6, py, TFT_MAGENTA);
  tft.drawLine(px, py - 6, px, py + 6, TFT_MAGENTA);
}

void setup() {
  Serial.begin(115200);
  delay(150);
  Serial.println();
  Serial.println("=== TFT Touch Test Start ===");

  tft.init();
  tft.setRotation(1); // Landscape 320x240
  drawStaticUi();

  touch.begin();
  touch.setRotation(1);

  Serial.println("If the screen is blank, verify TFT_eSPI User_Setup pin mapping.");
  Serial.println("If touch is wrong, adjust TOUCH_CS_PIN/TOUCH_IRQ_PIN and calibration.");
}

void loop() {
  if (millis() - lastUiUpdateMs > 800) {
    lastUiUpdateMs = millis();
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.fillRect(180, 10, 130, 18, TFT_BLACK);
    tft.setCursor(180, 10);
    tft.printf("%lus", millis() / 1000UL);
  }

  if (touch.touched()) {
    TS_Point p = touch.getPoint();
    drawTouchData(p.x, p.y, p.z);

    // Toggle message only once per press (edge detection).
    if (!lastTouched) {
      messageToggle = !messageToggle;
      if (messageToggle) {
        drawMessage("Scherm aangeraakt");
      } else {
        drawMessage("Nogmaals drukken...");
      }
    }
    lastTouched = true;

    if (millis() - lastTouchPrintMs > 120) {
      lastTouchPrintMs = millis();
      Serial.printf("Touch raw => X:%d Y:%d Z:%d\n", p.x, p.y, p.z);
    }
  } else {
    lastTouched = false;
  }
}
