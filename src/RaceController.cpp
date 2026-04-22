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
  seqStep = 0;
  lapTimes.clear();
}

bool RaceController::isRunning() { return running; }
bool RaceController::isSequence() { return sequence; }
int RaceController::getStep() { return seqStep; }

unsigned long RaceController::getRemaining() {
  if (sequence) {
    // During countdown sequence, return time remaining until start
    unsigned long elapsed = millis() - seqStart;
    return (elapsed >= 300000) ? 0 : 300000 - elapsed;
  }
  if (running) {
    // During race, return time remaining
    unsigned long elapsed = millis() - startTime;
    return (elapsed >= 300000) ? 0 : 300000 - elapsed;
  }
  return 0;
}

unsigned long RaceController::getElapsed() {
  if (running) {
    return millis() - startTime;
  }
  return 0;
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
    seqStep++;
    sequence = false;
    // Start race with elapsed time already at 5 minutes (300000ms)
    // so overtime counting continues from +0:00
    running = true;
    startTime = seqStart;  // Use sequence start time, not current time
  }
}

void RaceController::update() {
  stepSequence();
  // Race blijft lopen, geen auto-stop meer
}

// Lap time tracking
void RaceController::addLapTime() {
  if (running) {
    unsigned long elapsed = getElapsed();
    lapTimes.push_back(elapsed);
  }
}

void RaceController::clearLapTimes() {
  lapTimes.clear();
}

std::vector<unsigned long> RaceController::getLapTimes() {
  return lapTimes;
}
