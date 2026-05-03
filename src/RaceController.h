#pragma once
#include <Arduino.h>
#include <vector>

// Time-driven sequence event
struct SequenceEvent {
  unsigned long timestamp;     // Absolute time (millis)
  int remainingMinutes;        // Minutes remaining (for display/telegram)
  int hornDuration;            // Horn duration in ms (0 = no horn)
  bool hornDone;               // Horn signal completed
  bool telegramDone;           // Telegram notification sent
};

class RaceController {

public:
  void begin();
  void update();

  void startSequence();
  void startShortSequence();
  void cancel();

  bool isRunning();
  bool isSequence();
  bool isShortSequence();
  int getStep();

  unsigned long getRemaining();
  unsigned long getElapsed();
  
  // Lap time tracking
  void addLapTime();
  void clearLapTimes();
  std::vector<unsigned long> getLapTimes();

private:

  bool running = false;
  bool sequence = false;
  bool shortSequence = false;

  unsigned long startTime = 0;
  unsigned long seqStart = 0;

  // Time-driven sequence schedule
  SequenceEvent events[4];  // Max 4 events (5min: 5,4,1,0 or 3min: 3,2,1,0)
  int eventCount = 0;
  int currentEvent = 0;
  
  std::vector<unsigned long> lapTimes;

  void processSequenceEvents();
  void scheduleEvents(bool isShort);
  void startRace();

};
