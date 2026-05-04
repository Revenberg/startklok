#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>

// Known working wiring from this project
#define TFT_CS        5
#define TFT_DC        4
#define TFT_RST       19
#define TFT_BLK       15
#define TFT_SCK       18
#define TFT_MOSI      23
#define TFT_MISO      2
#define TOUCH_CS_PIN  13
#define TOUCH_IRQ_PIN 14

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen touch(TOUCH_CS_PIN, TOUCH_IRQ_PIN);

struct Btn {
  int x;
  int y;
  int w;
  int h;
  const char* label;
  uint16_t color;
  bool pressed;
};

Btn buttons[] = {
  {8,   45, 148, 55, "START", ILI9341_DARKGREEN, false},
  {164, 45, 148, 55, "STOP",  ILI9341_RED,       false},
  {8,  110, 148, 55, "HORN",  ILI9341_MAGENTA,   false},
  {164,110, 148, 55, "END",   ILI9341_ORANGE,    false},
  {8,  175,  72, 55, "R1",    ILI9341_NAVY,      false},
  {86, 175,  72, 55, "R2",    ILI9341_NAVY,      false},
  {164,175,  72, 55, "R3",    ILI9341_NAVY,      false},
  {242,175,  70, 55, "R4",    ILI9341_NAVY,      false}
};

const int buttonCount = sizeof(buttons) / sizeof(buttons[0]);
bool touchDown = false;
unsigned long lastTouchPrint = 0;

// Guided test order to diagnose mirrored/misaligned touch.
const int testOrder[] = {0, 1, 2, 3, 4, 7}; // START, STOP, HORN, END, R1, R4
const int testCount = sizeof(testOrder) / sizeof(testOrder[0]);
int testStep = 0;
int okCount = 0;
int failCount = 0;

bool contains(const Btn& b, int x, int y) {
  return x >= b.x && x < (b.x + b.w) && y >= b.y && y < (b.y + b.h);
}

void drawButton(const Btn& b) {
  uint16_t bg = b.pressed ? ILI9341_GREEN : b.color;
  tft.fillRect(b.x, b.y, b.w, b.h, bg);
  tft.drawRect(b.x, b.y, b.w, b.h, ILI9341_WHITE);

  tft.setTextSize(2);
  tft.setTextColor(ILI9341_WHITE);
  int tx = b.x + (b.w - ((int)strlen(b.label) * 12)) / 2;
  int ty = b.y + (b.h / 2) - 8;
  tft.setCursor(tx, ty);
  tft.print(b.label);
}

void drawExpectedHighlight() {
  if (testStep >= testCount) return;
  const Btn& e = buttons[testOrder[testStep]];
  tft.drawRect(e.x - 2, e.y - 2, e.w + 4, e.h + 4, ILI9341_YELLOW);
}

void drawUi() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_CYAN);
  tft.setCursor(8, 8);
  tft.print("TFT Button Test");

  tft.drawRect(8, 26, 304, 16, ILI9341_DARKCYAN);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(10, 30);
  tft.print("Druk op: ");
  tft.print(buttons[testOrder[testStep]].label);

  for (int i = 0; i < buttonCount; i++) {
    drawButton(buttons[i]);
  }

  drawExpectedHighlight();

  tft.drawRect(8, 235, 304, 20, ILI9341_DARKCYAN);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_LIGHTGREY);
  tft.setCursor(10, 241);
  tft.print("Serial: verwacht + gedetecteerd.");
}

void drawTestHeader() {
  tft.fillRect(9, 27, 302, 14, ILI9341_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(10, 30);
  if (testStep < testCount) {
    tft.print("Druk op: ");
    tft.print(buttons[testOrder[testStep]].label);
    tft.print("   OK:");
    tft.print(okCount);
    tft.print(" FOUT:");
    tft.print(failCount);
  } else {
    tft.print("Test klaar  OK:");
    tft.print(okCount);
    tft.print(" FOUT:");
    tft.print(failCount);
  }

  // Redraw button area highlights after header updates
  for (int i = 0; i < buttonCount; i++) {
    tft.drawRect(buttons[i].x, buttons[i].y, buttons[i].w, buttons[i].h, ILI9341_WHITE);
  }
  drawExpectedHighlight();
}

void reportStepResult(int detectedIndex, int sx, int sy, int rx, int ry, int rz) {
  const int expectedIndex = testOrder[testStep];
  const char* expected = buttons[expectedIndex].label;
  const char* detected = detectedIndex >= 0 ? buttons[detectedIndex].label : "NONE";

  bool pass = (detectedIndex == expectedIndex);
  if (pass) {
    okCount++;
    Serial.printf("[STEP %d OK] verwacht=%s gedetecteerd=%s M:%d,%d R:%d,%d Z:%d\n",
                  testStep + 1, expected, detected, sx, sy, rx, ry, rz);
  } else {
    failCount++;
    Serial.printf("[STEP %d FOUT] verwacht=%s gedetecteerd=%s M:%d,%d R:%d,%d Z:%d\n",
                  testStep + 1, expected, detected, sx, sy, rx, ry, rz);

    // Useful hint for mirror diagnosis on the first row.
    if ((expectedIndex == 0 && detectedIndex == 1) || (expectedIndex == 1 && detectedIndex == 0)) {
      Serial.println("[HINT] Lijkt horizontaal gespiegeld (X-as omgekeerd).");
    }
    if ((expectedIndex == 2 && detectedIndex == 3) || (expectedIndex == 3 && detectedIndex == 2)) {
      Serial.println("[HINT] Rij 2 links/rechts omgewisseld: controleer X mapping.");
    }
  }

  if (testStep < testCount) {
    testStep++;
  }
  delay(2000);  // wacht 2 seconden voor de volgende stap
  drawTestHeader();
}

bool readTouch(int& sx, int& sy, int& rx, int& ry, int& rz) {
  if (!touch.touched()) {
    return false;
  }

  TS_Point p = touch.getPoint();
  rx = p.x;
  ry = p.y;
  rz = p.z;

  if (p.z < 100 || p.z > 3500) {
    return false;
  }

  // Confirmed mapping: p.x → screen X (normal), p.y → screen Y (inverted: top=high raw).
  const int rawMin = 200;
  const int rawMax = 3900;
  sx = map(constrain(p.x, rawMin, rawMax), rawMin, rawMax, 0, 319);
  sy = map(constrain(p.y, rawMin, rawMax), rawMin, rawMax, 239, 0);

  return true;
}

void showTouchPoint(int sx, int sy, int rx, int ry, int rz) {
  tft.fillRect(8, 210, 304, 23, ILI9341_BLACK);
  tft.drawRect(8, 210, 304, 23, ILI9341_DARKGREY);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setCursor(10, 216);
  tft.printf("M:%3d,%3d  R:%4d,%4d Z:%4d", sx, sy, rx, ry, rz);

  if (sx >= 3 && sx < 317 && sy >= 40 && sy < 208) {
    tft.fillCircle(sx, sy, 3, ILI9341_RED);
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("=== TFT Button Test ===");

  pinMode(TFT_BLK, OUTPUT);
  digitalWrite(TFT_BLK, HIGH);

  SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI, TFT_CS);

  tft.begin(4000000);
  tft.setRotation(1);
  tft.invertDisplay(false);

  touch.begin();
  touch.setRotation(1);

  drawUi();
  drawTestHeader();
  Serial.println("UI ready. Guided test started.");
  Serial.println("Druk op de knop die bovenaan staat.");
}

void loop() {
  int sx = 0, sy = 0, rx = 0, ry = 0, rz = 0;
  bool touched = readTouch(sx, sy, rx, ry, rz);

  if (touched && !touchDown) {
    touchDown = true;

    showTouchPoint(sx, sy, rx, ry, rz);

    int hitIndex = -1;
    for (int i = 0; i < buttonCount; i++) {
      if (contains(buttons[i], sx, sy)) {
        hitIndex = i;
        buttons[i].pressed = !buttons[i].pressed;
        drawButton(buttons[i]);
        Serial.printf("[HIT] %s  M:%d,%d  R:%d,%d Z:%d\n", buttons[i].label, sx, sy, rx, ry, rz);
        break;
      }
    }

    if (hitIndex < 0) {
      Serial.printf("[MISS] M:%d,%d  R:%d,%d Z:%d\n", sx, sy, rx, ry, rz);
    }

    if (testStep < testCount) {
      reportStepResult(hitIndex, sx, sy, rx, ry, rz);
    }
  }

  if (!touched) {
    touchDown = false;
  }

  if (touched && millis() - lastTouchPrint > 120) {
    lastTouchPrint = millis();
    Serial.printf("[TOUCH] M:%d,%d  R:%d,%d Z:%d\n", sx, sy, rx, ry, rz);
  }
}
