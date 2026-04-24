#include <Arduino.h>
#include "relay.h"

// pins
const int RELAY_1 = 32;
const int RELAY_2 = 33;
const int RELAY_3 = 25;
const int RELAY_4 = 26;

void relayInit() {

  pinMode(RELAY_1, OUTPUT);
  pinMode(RELAY_2, OUTPUT);
  pinMode(RELAY_3, OUTPUT);
  pinMode(RELAY_4, OUTPUT);

  relayReset();
}

void relayReset() {

  digitalWrite(RELAY_1, LOW);
  digitalWrite(RELAY_2, LOW);
  digitalWrite(RELAY_3, LOW);
  digitalWrite(RELAY_4, LOW);
}

void relaySet(int nr, int state) {

  int pin;

  switch(nr) {
    case 1: pin = RELAY_1; break;
    case 2: pin = RELAY_2; break;
    case 3: pin = RELAY_3; break;
    case 4: pin = RELAY_4; break;
    default: return;
  }

  digitalWrite(pin, state ? HIGH : LOW);
}
