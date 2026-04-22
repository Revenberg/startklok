#include <Arduino.h>
#include "lcd.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

static LiquidCrystal_I2C lcd(0x27, 16, 2);

void lcdInit() {
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
}

void lcdShowIdle(String mode, String ssid) {
  lcd.setCursor(0,0);
  lcd.print("READY           ");
  lcd.setCursor(0,1);
  lcd.print(mode + " " + ssid + "     ");
}

void lcdShowRace(unsigned long remaining) {

  int sec = remaining / 1000;
  int min = sec / 60;
  int r   = sec % 60;

  char buf[17];
  sprintf(buf,"%02d:%02d RUN",min,r);

  lcd.setCursor(0,1);
  lcd.print(buf);
}
