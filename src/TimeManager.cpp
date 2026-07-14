#include "TimeManager.h"
#include <WiFi.h>
#include "Config.h"
#include "CoreLogic.h"
#include "Logger.h"
#include "constants.h"

bool TimeManager::synchronized_ = false;
time_t TimeManager::lastSyncEpoch_ = 0;

void TimeManager::begin() {
  if (!WiFi.isConnected()) return;
  configTzTime(Config::timezone, Constants::NTP_SERVER_1, Constants::NTP_SERVER_2);
  Logger::info("NTP synchronization started");
}

void TimeManager::loop() {
  const time_t current = time(nullptr);
  const bool valid = CoreLogic::isSynchronizedEpoch(static_cast<int64_t>(current));
  if (valid) {
    lastSyncEpoch_ = current;
    if (!synchronized_) Logger::info("System time synchronized");
  }
  synchronized_ = valid;
}

bool TimeManager::synchronized() { return synchronized_; }
time_t TimeManager::now() { return synchronized_ ? time(nullptr) : 0; }
time_t TimeManager::lastSyncEpoch() { return lastSyncEpoch_; }
