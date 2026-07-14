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
float UsageTracker::cumulativeEnergyKwh_ = -1.0f;
float UsageTracker::dayVariableCost_ = 0.0f;
float UsageTracker::monthVariableCost_ = 0.0f;
float UsageTracker::yearVariableCost_ = 0.0f;
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
  cumulativeEnergyKwh_ = preferences_.getFloat("energy_kwh", -1.0f);
  dayVariableCost_ = preferences_.getFloat("day_var_cost", 0.0f);
  monthVariableCost_ = preferences_.getFloat("month_var_cost", 0.0f);
  yearVariableCost_ = preferences_.getFloat("year_var_cost", 0.0f);
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
  preferences_.putFloat("energy_kwh", cumulativeEnergyKwh_);
  preferences_.putFloat("day_var_cost", dayVariableCost_);
  preferences_.putFloat("month_var_cost", monthVariableCost_);
  preferences_.putFloat("year_var_cost", yearVariableCost_);
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
    dayVariableCost_ = 0.0f;
    strlcpy(dayKey_, newDay, sizeof(dayKey_));
    changed = true;
  }
  if (monthBaseline_ < 0 || strcmp(monthKey_, newMonth) != 0) {
    monthBaseline_ = volume;
    monthVariableCost_ = 0.0f;
    strlcpy(monthKey_, newMonth, sizeof(monthKey_));
    changed = true;
  }
  if (yearBaseline_ < 0 || strcmp(yearKey_, newYear) != 0) {
    yearBaseline_ = volume;
    yearVariableCost_ = 0.0f;
    strlcpy(yearKey_, newYear, sizeof(yearKey_));
    changed = true;
  }
  if (changed) saveBaselines();
}

void UsageTracker::update(float rawVolumeM3, time_t now) {
  const float correctedVolume = rawVolumeM3 + Config::meterOffsetM3;
  const bool meterReset = previousVolume_ >= 0.0f && correctedVolume + 0.001f < previousVolume_;
  if (meterReset) {
    Logger::warn("Meter value decreased; treating as replacement/reset");
    dayBaseline_ = monthBaseline_ = yearBaseline_ = correctedVolume;
    dayVariableCost_ = monthVariableCost_ = yearVariableCost_ = 0.0f;
  }
  updatePeriods(correctedVolume, now);

  const TariffPeriod& tariff = Config::activeTariff(now);
  const float energyFactor = Config::calorificValue * Config::correctionFactor;
  float positiveDelta = 0.0f;
  if (!meterReset && previousVolume_ >= 0.0f && correctedVolume >= previousVolume_) {
    positiveDelta = correctedVolume - previousVolume_;
  }

  if (cumulativeEnergyKwh_ < 0.0f) cumulativeEnergyKwh_ = correctedVolume * energyFactor;
  else cumulativeEnergyKwh_ += positiveDelta * energyFactor;

  const float variableCostIncrement = positiveDelta * energyFactor * tariff.workPricePerKwh;
  dayVariableCost_ += variableCostIncrement;
  monthVariableCost_ += variableCostIncrement;
  yearVariableCost_ += variableCostIncrement;

  snapshot_.rawVolumeM3 = rawVolumeM3;
  snapshot_.correctedVolumeM3 = correctedVolume;
  snapshot_.energyKwh = cumulativeEnergyKwh_;
  snapshot_.timestamp = now;
  if (!meterReset && previousVolume_ >= 0.0f && previousTimestamp_ > 0 && now > previousTimestamp_) {
    snapshot_.flowM3h = CoreLogic::calculateFlowM3h(previousVolume_, correctedVolume, static_cast<uint32_t>(now - previousTimestamp_), Config::maxFlowM3h);
  } else {
    snapshot_.flowM3h = 0.0f;
  }

  snapshot_.dayM3 = correctedVolume > dayBaseline_ ? correctedVolume - dayBaseline_ : 0.0f;
  snapshot_.monthM3 = correctedVolume > monthBaseline_ ? correctedVolume - monthBaseline_ : 0.0f;
  snapshot_.yearM3 = correctedVolume > yearBaseline_ ? correctedVolume - yearBaseline_ : 0.0f;
  snapshot_.dayCost = dayVariableCost_ + tariff.basePriceMonthly / 30.4375f;
  snapshot_.monthCost = monthVariableCost_ + tariff.basePriceMonthly;
  snapshot_.yearCost = yearVariableCost_ + tariff.basePriceMonthly * 12.0f;

  previousVolume_ = correctedVolume;
  previousTimestamp_ = now;
  static uint32_t lastPersist = 0;
  if (millis() - lastPersist >= 3600000UL) {
    saveBaselines();
    lastPersist = millis();
  }
}

const UsageSnapshot& UsageTracker::snapshot() { return snapshot_; }
