#include "RaceController.h"
#include "telegram.h"
#include <RTClib.h>

extern RTC_DS3231 rtc;
extern void hornStart(int ms);  // Forward declaration from main.cpp

void RaceController::begin() {
  running = false;
  sequence = false;
}

void RaceController::scheduleEvents(bool isShort) {
  unsigned long now = millis();
  
  if (isShort) {
    // 3-minute sequence: signals at 3, 2, 1, 0
    eventCount = 4;
    
    events[0] = {now, 3, false, false};              // T+0ms: 3 minutes remaining
    events[1] = {now + 60000, 2, false, false};      // T+60s: 2 minutes remaining
    events[2] = {now + 120000, 1, false, false};     // T+120s: 1 minute remaining
    events[3] = {now + 180000, 0, false, false};     // T+180s: START!
  } else {
    // 5-minute sequence: signals at 5, 4, 1, 0
    eventCount = 4;
    
    events[0] = {now, 5, false, false};              // T+0ms: 5 minutes remaining
    events[1] = {now + 60000, 4, false, false};      // T+60s: 4 minutes remaining
    events[2] = {now + 240000, 1, false, false};     // T+240s: 1 minute remaining
    events[3] = {now + 300000, 0, false, false};     // T+300s: START!
  }
  
  currentEvent = 0;
  
  Serial.println("[RACE] Time-driven sequence scheduled:");
  for (int i = 0; i < eventCount; i++) {
    Serial.printf("  Event %d: T+%lu ms (%d min remaining)\n", 
                  i, events[i].timestamp - now, events[i].remainingMinutes);
  }
}

void RaceController::startSequence() {
  sequence = true;
  shortSequence = false;
  seqStart = millis();
  scheduleEvents(false);  // 5-minute schedule
}

void RaceController::startShortSequence() {
  sequence = true;
  shortSequence = true;
  seqStart = millis();
  scheduleEvents(true);   // 3-minute schedule
}

void RaceController::cancel() {
  running = false;
  sequence = false;
  shortSequence = false;
  currentEvent = 0;
  eventCount = 0;
  lapTimes.clear();
}

bool RaceController::isRunning() { return running; }
bool RaceController::isSequence() { return sequence; }
bool RaceController::isShortSequence() { return shortSequence; }
int RaceController::getStep() { return currentEvent; }

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

void RaceController::processSequenceEvents() {
  if (!sequence || currentEvent >= eventCount) return;
  
  unsigned long now = millis();
  SequenceEvent &event = events[currentEvent];
  
  // Check if this event's time has arrived
  if (now >= event.timestamp) {
    
    // PRIORITY 1: Horn signal (must be precise)
    if (!event.hornDone) {
      hornStart(2000);  // 2 second horn
      event.hornDone = true;
      Serial.printf("[RACE] 🔊 Horn signal at %d minutes remaining\n", event.remainingMinutes);
    }
    
    // PRIORITY 2: Telegram notification (async, can be delayed)
    if (!event.telegramDone) {
      String message;
      
      if (event.remainingMinutes == 0) {
        // START event - include RTC time
        DateTime rtcNow = rtc.now();
        char timeStr[32];
        sprintf(timeStr, "%02d:%02d:%02d", rtcNow.hour(), rtcNow.minute(), rtcNow.second());
        message = "🏁 RACE START!\n⏰ Start tijd: " + String(timeStr);
        
        // Race is starting!
        sequence = false;
        shortSequence = false;
        running = true;
        startTime = seqStart;
        
        Serial.println("[RACE] ✅ Race started!");
      } else {
        // Countdown event
        message = "⏱️ [SEQ] " + String(event.remainingMinutes) + " minute";
        if (event.remainingMinutes > 1) message += "s";
        message += " remaining";
      }
      
      telegram.sendMessage(message);
      event.telegramDone = true;
      Serial.printf("[RACE] 📱 Telegram sent: %d min remaining\n", event.remainingMinutes);
    }
    
    // Move to next event
    currentEvent++;
  }
}

void RaceController::update() {
  processSequenceEvents();
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
