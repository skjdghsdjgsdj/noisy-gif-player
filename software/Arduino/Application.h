#pragma once
#include <Arduino.h>

class Application {
public:
  Application();

  void setup();
  void loopOnce();

private:
  [[noreturn]] void enterDeepSleep();      // High-level entry point for deep sleep

  void setupPinsAndPower();
  void setupDisplay();
  void setupStorage();
  void seedRandom();
  void setupSubsystems();

  void fadeBacklightOff();
  void powerDownPeripherals();
  [[noreturn]] void startESPDeepSleep();
};

