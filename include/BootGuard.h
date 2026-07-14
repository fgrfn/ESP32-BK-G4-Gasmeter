#pragma once

#include <Arduino.h>

class BootGuard {
 public:
  static void begin();
  static void loop(bool networkHealthy, bool timeSynchronized, bool mbusHealthy);
  static bool safeMode();
  static bool stable();
  static bool pendingOtaValidation();
  static uint8_t failureCount();
  static void markPlannedRestart(const char* reason);

 private:
  static bool isFailureResetReason(int reason);
  static bool safeMode_;
  static bool stable_;
  static bool pendingOtaValidation_;
  static uint8_t failureCount_;
};
