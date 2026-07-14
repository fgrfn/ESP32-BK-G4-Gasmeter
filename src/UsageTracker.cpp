#include "UsageTracker.h"
#include <time.h>
#include "Config.h"
#include "CoreLogic.h"
#include "Logger.h"

Preferences UsageTracker::preferences_;
UsageSnapshot UsageTracker::snapshot_;
float UsageTracker::previousVolume_ = -1.0f;
time_t UsageTracker::previousTimestamp_ = 0;
float UsageTracker::dayBaseline_ = -1.0f;
float UsageTracker::monthBaseline_ = -1.0f;
float UsageTracker::yearBaseline_ = -1.0f;
char UsageTracker::dayKey_[11] = "";
char UsageTracker::monthKey_[8] = "";
char UsageTracker::yearKey_[5] = "";

void UsageTracker::begin() { loadBaselines(); }

void UsageTracker::loadBaselines() {
  preferences_.begin("usage", true);
  dayBaseline_ = preferences_.getFloat("day_base", -1.0f);
  monthBaseline_ = preferences_.getFloat("month_base", -1.0f);
  yearBaseline_ = preferences_.getFloat("year_base", -1.0f);
  preferences_.getString("day_key", dayKey_, sizeof(dayKey_));
  preferences_.getString("month_key", monthKey_, sizeof(monthKey_));
  preferences_.getString("year_key", yearKey_, sizeof(yearKey_));
  previousVolume_ = preferences_.getFloat("last_volume", -1.0f);
  previousTimestamp_ = static_cast<time_t>(preferences_.getULong64("last_time", 0));
  preferences_.end();
}

void UsageTracker::saveBaselines() {
  preferences_.begin("usage", false);
  preferences_.putFloat("day_base", dayBaseline_);
  preferences_.putFloat("month_base", monthBaseline_);
  preferences_.putFloat("year_base", yearBaseline_);
  preferences_.putString("day_key", dayKey_);
  preferences_.putString("month_key", monthKey_);
  preferences_.putString("year_key", yearKey_);
  preferences_.putFloat("last_volume", previousVolume_);
  preferences_.putULong64("last_time", static_cast<uint64_t>(previousTimestamp_));
  preferences_.end();
}

void UsageTracker::keyForTime(time_t now, char* dayKey, size_t daySize, char* monthKey, size_t monthSize, char* yearKey, size_t yearSize) {
  struct tm local = {};
  localtime_r(&now, &local);
  strftime(dayKey, daySize, "%Y-%m-%d", &local);
  strftime(monthKey, monthSize, "%Y-%m", &local);
  strftime(yearKey, yearSize, "%Y", &local);
}

void UsageTracker::updatePeriods(float volume, time_t now) {
  char newDay[11], newMonth[8], newYear[5];
  keyForTime(now, newDay, sizeof(newDay), newMonth, sizeof(newMonth), newYear, sizeof(newYear));
  bool changed = false;
  if (dayBaseline_ < 0 || strcmp(dayKey_, newDay) != 0) {
    dayBaseline_ = volume;
    strlcpy(dayKey_, newDay, sizeof(dayKey_));
    changed = true;
  }
  if (monthBaseline_ < 0 || strcmp(monthKey_, newMonth) != 0) {
    monthBaseline_ = volume;
    strlcpy(monthKey_, newMonth, sizeof(monthKey_));
    changed = true;
  }
  if (yearBaseline_ < 0 || strcmp(yearKey_, newYear) != 0) {
    yearBaseline_ = volume;
    strlcpy(yearKey_, newYear, sizeof(yearKey_));
    changed = true;
  }
  if (changed) saveBaselines();
}

void UsageTracker::update(float rawVolumeM3, time_t now) {
  const float correctedVolume = rawVolumeM3 + Config::meterOffsetM3;
  if (previousVolume_ >= 0.0f && correctedVolume + 0.001f < previousVolume_) {
    Logger::warn("Meter value decreased; treating as replacement/reset");
    dayBaseline_ = monthBaseline_ = yearBaseline_ = correctedVolume;
  }
  updatePeriods(correctedVolume, now);

  snapshot_.rawVolumeM3 = rawVolumeM3;
  snapshot_.correctedVolumeM3 = correctedVolume;
  snapshot_.energyKwh = correctedVolume * Config::calorificValue * Config::correctionFactor;
  snapshot_.timestamp = now;
  if (previousVolume_ >= 0.0f && previousTimestamp_ > 0 && now > previousTimestamp_) {
    snapshot_.flowM3h = CoreLogic::calculateFlowM3h(previousVolume_, correctedVolume, static_cast<uint32_t>(now - previousTimestamp_), Config::maxFlowM3h);
  } else {
    snapshot_.flowM3h = 0.0f;
  }

  snapshot_.dayM3 = correctedVolume > dayBaseline_ ? correctedVolume - dayBaseline_ : 0.0f;
  snapshot_.monthM3 = correctedVolume > monthBaseline_ ? correctedVolume - monthBaseline_ : 0.0f;
  snapshot_.yearM3 = correctedVolume > yearBaseline_ ? correctedVolume - yearBaseline_ : 0.0f;
  const TariffPeriod& tariff = Config::activeTariff(now);
  const float multiplier = Config::calorificValue * Config::correctionFactor * tariff.workPricePerKwh;
  snapshot_.dayCost = snapshot_.dayM3 * multiplier + tariff.basePriceMonthly / 30.4375f;
  snapshot_.monthCost = snapshot_.monthM3 * multiplier + tariff.basePriceMonthly;
  snapshot_.yearCost = snapshot_.yearM3 * multiplier + tariff.basePriceMonthly * 12.0f;

  previousVolume_ = correctedVolume;
  previousTimestamp_ = now;
  static uint32_t lastPersist = 0;
  if (millis() - lastPersist >= 3600000UL) {
    saveBaselines();
    lastPersist = millis();
  }
}

const UsageSnapshot& UsageTracker::snapshot() { return snapshot_; }
