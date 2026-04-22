#pragma once
#include <Arduino.h>

class RaceController {

public:
  void begin();
  void update();

  void startSequence();
  void cancel();

  bool isRunning();
  bool isSequence();

  unsigned long getRemaining();

private:

  bool running = false;
  bool sequence = false;

  unsigned long startTime = 0;
  unsigned long seqStart = 0;

  int seqStep = 0;

  void stepSequence();
  void startRace();

};
