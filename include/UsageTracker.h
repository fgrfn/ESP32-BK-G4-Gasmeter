#pragma once

#include <Arduino.h>
#include <Preferences.h>

struct UsageSnapshot {
  float rawVolumeM3 = -1.0f;
  float correctedVolumeM3 = -1.0f;
  float energyKwh = 0.0f;
  float flowM3h = 0.0f;
  float dayM3 = 0.0f;
  float monthM3 = 0.0f;
  float yearM3 = 0.0f;
  float dayEnergyKwh = 0.0f;
  float monthEnergyKwh = 0.0f;
  float yearEnergyKwh = 0.0f;
  uint32_t continuousFlowMinutes = 0;
  bool continuousFlowAlert = false;
  bool timeSynchronized = false;
  time_t timestamp = 0;
};

struct DailyUsageRecord {
  char date[11] = "";
  float volumeM3 = 0.0f;
  float energyKwh = 0.0f;
};

class UsageTracker {
 public:
  static void begin();
  static void update(float rawVolumeM3, time_t now, bool timeSynchronized);
  static const UsageSnapshot& snapshot();
  static const DailyUsageRecord* dailyHistory(size_t& count);

 private:
  static void loadState();
  static void saveState();
  static void loadHistory();
  static void saveHistory();
  static void addDailyRecord(const char* date, float volumeM3, float energyKwh);
  static void updatePeriods(float volume, float energy, time_t now);
  static void updateContinuousFlow(float flowM3h);
  static Preferences preferences_;
  static UsageSnapshot snapshot_;
  static float previousVolume_;
  static uint32_t previousReadingMs_;
  static float dayBaseline_;
  static float monthBaseline_;
  static float yearBaseline_;
  static float dayEnergyBaseline_;
  static float monthEnergyBaseline_;
  static float yearEnergyBaseline_;
  static float cumulativeEnergyKwh_;
  static uint32_t continuousFlowStartedMs_;
  static DailyUsageRecord dailyHistory_[31];
  static size_t dailyHistoryCount_;
  static char dayKey_[11];
  static char monthKey_[8];
  static char yearKey_[5];
};
