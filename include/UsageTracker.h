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
  float dayCost = 0.0f;
  float monthCost = 0.0f;
  float yearCost = 0.0f;
  time_t timestamp = 0;
};

class UsageTracker {
 public:
  static void begin();
  static void update(float rawVolumeM3, time_t now);
  static const UsageSnapshot& snapshot();

 private:
  static void loadBaselines();
  static void saveBaselines();
  static void updatePeriods(float volume, time_t now);
  static void keyForTime(time_t now, char* dayKey, size_t daySize, char* monthKey, size_t monthSize, char* yearKey, size_t yearSize);
  static Preferences preferences_;
  static UsageSnapshot snapshot_;
  static float previousVolume_;
  static time_t previousTimestamp_;
  static float dayBaseline_;
  static float monthBaseline_;
  static float yearBaseline_;
  static float cumulativeEnergyKwh_;
  static float dayVariableCost_;
  static float monthVariableCost_;
  static float yearVariableCost_;
  static char dayKey_[11];
  static char monthKey_[8];
  static char yearKey_[5];
};
