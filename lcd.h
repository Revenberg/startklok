#pragma once
#include <Arduino.h>

void lcdInit();
void lcdShowIdle(String mode, String ssid);
void lcdShowRace(unsigned long remaining);