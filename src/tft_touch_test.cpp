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

  // Color bars to quickly verify RGB output and orientation.
  tft.fillRect(8, 110, 60, 40, TFT_RED);
  tft.fillRect(72, 110, 60, 40, TFT_GREEN);
  tft.fillRect(136, 110, 60, 40, TFT_BLUE);
  tft.fillRect(200, 110, 60, 40, TFT_YELLOW);
  tft.fillRect(264, 110, 48, 40, TFT_CYAN);

  tft.drawRect(8, 165, 304, 146, TFT_DARKGREY);
  tft.setCursor(14, 172);
  tft.println("Raw touch:");
}

void drawTouchData(int x, int y, int z) {
  tft.fillRect(14, 194, 290, 40, TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(14, 194);
  tft.printf("X: %4d  Y: %4d  Z: %4d", x, y, z);

  // Draw a marker in the touch area (mapped from raw range).
  int px = map(x, 200, 3800, 10, 302);
  int py = map(y, 200, 3800, 167, 309);
  px = constrain(px, 10, 302);
  py = constrain(py, 167, 309);

  tft.fillRect(10, 236, 300, 72, TFT_BLACK);
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
