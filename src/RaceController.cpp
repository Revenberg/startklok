#include "RaceController.h"
#include "telegram.h"

void RaceController::begin() {
  running = false;
  sequence = false;
}

void RaceController::startSequence() {
  sequence = true;
  shortSequence = false;
  seqStep = 0;
  seqStart = millis();
}

void RaceController::startShortSequence() {
  sequence = true;
  shortSequence = true;
  seqStep = 0;
  seqStart = millis();
}

void RaceController::cancel() {
  running = false;
  sequence = false;
  shortSequence = false;
  seqStep = 0;
  lapTimes.clear();
}

bool RaceController::isRunning() { return running; }
bool RaceController::isSequence() { return sequence; }
bool RaceController::isShortSequence() { return shortSequence; }
int RaceController::getStep() { return seqStep; }

unsigned long RaceController::getRemaining() {
  unsigned long duration = shortSequence ? 180000 : 300000; // 3 min or 5 min
  
  if (sequence) {
    // During countdown sequence, return time remaining until start
    unsigned long elapsed = millis() - seqStart;
    return (elapsed >= duration) ? 0 : duration - elapsed;
  }
  if (running) {
    // During race, return time remaining (always 5 minutes for race)
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

  if (shortSequence) {
    // Short sequence: 3 min countdown (signals at 3, 2, 1, 0)
    if (seqStep == 0 && t >= 0) {
      telegram.sendMessage("[SEQ] 3 minutes remaining");
      seqStep++;
    }
    if (seqStep == 1 && t >= 60000) {  // 2 min mark
      telegram.sendMessage("[SEQ] 2 minutes remaining");
      seqStep++;
    }
    if (seqStep == 2 && t >= 120000) {  // 1 min mark
      telegram.sendMessage("[SEQ] 1 minute remaining");
      seqStep++;
    }
   if (seqStep == 3 && t >= 180000) {  // START (0 min)
      telegram.sendMessage("[SEQ] START!");
      seqStep++;
      sequence = false;
      shortSequence = false;
      running = true;
      startTime = seqStart;
    }
  } else {
    // Normal sequence: 5 min countdown (signals at 5, 4, 1, 0)
    if (seqStep == 0 && t >= 0) {
    telegram.sendMessage("[SEQ] 5 minutes remaining");
      seqStep++;
    }
    if (seqStep == 1 && t >= 60000) {  // 4 min mark
      telegram.sendMessage("[SEQ] 4 minutes remaining");
      seqStep++;
    }
    if (seqStep == 2 && t >= 240000) {  // 1 min mark
      telegram.sendMessage("[SEQ] 1 minute remaining");
      seqStep++;
    }
    if (seqStep == 3 && t >= 300000) {  // START (0 min)
      telegram.sendMessage("[SEQ] START!");
      seqStep++;
      sequence = false;
      running = true;
      startTime = seqStart;
    }
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
