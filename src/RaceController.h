#pragma once
#include <Arduino.h>
#include <vector>

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

  int seqStep = 0;
  
  std::vector<unsigned long> lapTimes;

  void stepSequence();
  void startRace();

};
