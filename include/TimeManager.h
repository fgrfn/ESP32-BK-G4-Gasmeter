#pragma once

#include <Arduino.h>
#include <time.h>

class TimeManager {
 public:
  static void begin();
  static void loop();
  static bool synchronized();
  static time_t now();
  static time_t lastSyncEpoch();

 private:
  static bool synchronized_;
  static time_t lastSyncEpoch_;
};
