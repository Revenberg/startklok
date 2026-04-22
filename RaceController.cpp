#include "RaceController.h"

void RaceController::begin() {
  running = false;
  sequence = false;
}

void RaceController::startSequence() {
  sequence = true;
  seqStep = 0;
  seqStart = millis();
}

void RaceController::cancel() {
  running = false;
  sequence = false;
}

bool RaceController::isRunning() { return running; }
bool RaceController::isSequence() { return sequence; }

unsigned long RaceController::getRemaining() {
  if (!running) return 0;
  unsigned long elapsed = millis() - startTime;
  return (elapsed >= 300000) ? 0 : 300000 - elapsed;
}

void RaceController::startRace() {
  running = true;
  startTime = millis();
}

void RaceController::stepSequence() {

  if (!sequence) return;

  unsigned long t = millis() - seqStart;

  if (seqStep == 0 && t >= 0) {
    seqStep++;
  }

  if (seqStep == 1 && t >= 60000) {
    seqStep++;
  }

  if (seqStep == 2 && t >= 240000) {
    seqStep++;
  }

  if (seqStep == 3 && t >= 300000) {
    sequence = false;
    startRace();
  }
}

void RaceController::update() {
  stepSequence();

  if (running && getRemaining() == 0) {
    running = false;
  }
}