#include <Arduino.h>
#include "Application.h"

Application &appInstance() {
  static Application app;
  return app;
}

void setup() {
  appInstance().setup();
}

void loop() {
  appInstance().loopOnce();
}
