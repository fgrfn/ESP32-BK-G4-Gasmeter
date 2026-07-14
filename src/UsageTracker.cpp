#include "UsageTracker.h"
#include <ArduinoJson.h>
#include <time.h>
#include "Config.h"
#include "CoreLogic.h"
#include "Logger.h"
#include "UsageLogic.h"

Preferences UsageTracker::preferences_;
UsageSnapshot UsageTracker::snapshot_;
float UsageTracker::previousVolume_ = -1.0f;
float UsageTracker::flowReferenceVolume_ = -1.0f;
uint32_t UsageTracker::flowReferenceMs_ = 0;
float UsageTracker::dayBaseline_ = -1.0f;
float UsageTracker::monthBaseline_ = -1.0f;
float UsageTracker::yearBaseline_ = -1.0f;
float UsageTracker::dayEnergyBaseline_ = -1.0f;
float UsageTracker::monthEnergyBaseline_ = -1.0f;
float UsageTracker::yearEnergyBaseline_ = -1.0f;
float UsageTracker::cumulativeEnergyKwh_ = -1.0f;
uint32_t UsageTracker::continuousFlowStartedMs_ = 0;
uint32_t UsageTracker::lastPositiveFlowMs_ = 0;
DailyUsageRecord UsageTracker::dailyHistory_[31];
size_t UsageTracker::dailyHistoryCount_ = 0;
char UsageTracker::dayKey_[11] = "";
char UsageTracker::monthKey_[8] = "";
char UsageTracker::yearKey_[5] = "";

void UsageTracker::begin() {
  loadState();
  loadHistory();
}

void UsageTracker::loadState() {
  preferences_.begin("usage", true);
  dayBaseline_ = preferences_.getFloat("day_base", -1.0f);
  monthBaseline_ = preferences_.getFloat("month_base", -1.0f);
  yearBaseline_ = preferences_.getFloat("year_base", -1.0f);
  dayEnergyBaseline_ = preferences_.getFloat("day_energy", -1.0f);
  monthEnergyBaseline_ = preferences_.getFloat("month_energy", -1.0f);
  yearEnergyBaseline_ = preferences_.getFloat("year_energy", -1.0f);
  preferences_.getString("day_key", dayKey_, sizeof(dayKey_));
  preferences_.getString("month_key", monthKey_, sizeof(monthKey_));
  preferences_.getString("year_key", yearKey_, sizeof(yearKey_));
  previousVolume_ = preferences_.getFloat("last_volume", -1.0f);
  cumulativeEnergyKwh_ = preferences_.getFloat("energy_kwh", -1.0f);
  preferences_.end();

  preferences_.begin("usage", false);
  preferences_.remove("last_time");
  preferences_.remove("day_var_cost");
  preferences_.remove("month_var_cost");
  preferences_.remove("year_var_cost");
  preferences_.end();
}

void UsageTracker::saveState() {
  preferences_.begin("usage", false);
  preferences_.putFloat("day_base", dayBaseline_);
  preferences_.putFloat("month_base", monthBaseline_);
  preferences_.putFloat("year_base", yearBaseline_);
  preferences_.putFloat("day_energy", dayEnergyBaseline_);
  preferences_.putFloat("month_energy", monthEnergyBaseline_);
  preferences_.putFloat("year_energy", yearEnergyBaseline_);
  preferences_.putString("day_key", dayKey_);
  preferences_.putString("month_key", monthKey_);
  preferences_.putString("year_key", yearKey_);
  preferences_.putFloat("last_volume", previousVolume_);
  preferences_.putFloat("energy_kwh", cumulativeEnergyKwh_);
  preferences_.end();
}

void UsageTracker::loadHistory() {
  preferences_.begin("usage", true);
  const String historyJson = preferences_.getString("daily_history", "");
  preferences_.end();
  if (historyJson.isEmpty()) return;

  JsonDocument document;
  if (deserializeJson(document, historyJson)) return;
  JsonArrayConst values = document.as<JsonArrayConst>();
  dailyHistoryCount_ = 0;
  for (JsonObjectConst value : values) {
    if (dailyHistoryCount_ >= 31) break;
    const char* date = value["date"] | "";
    if (!CoreLogic::isValidIsoDate(date)) continue;
    DailyUsageRecord& record = dailyHistory_[dailyHistoryCount_++];
    strlcpy(record.date, date, sizeof(record.date));
    record.volumeM3 = value["volume_m3"] | 0.0f;
    record.energyKwh = value["energy_kwh"] | 0.0f;
  }
}

void UsageTracker::saveHistory() {
  JsonDocument document;
  JsonArray values = document.to<JsonArray>();
  for (size_t i = 0; i < dailyHistoryCount_; ++i) {
    JsonObject value = values.add<JsonObject>();
    value["date"] = dailyHistory_[i].date;
    value["volume_m3"] = dailyHistory_[i].volumeM3;
    value["energy_kwh"] = dailyHistory_[i].energyKwh;
  }
  String historyJson;
  serializeJson(document, historyJson);
  preferences_.begin("usage", false);
  preferences_.putString("daily_history", historyJson);
  preferences_.end();
}

void UsageTracker::addDailyRecord(const char* date, float volumeM3, float energyKwh) {
  if (!CoreLogic::isValidIsoDate(date)) return;
  if (dailyHistoryCount_ == 31) {
    for (size_t i = 1; i < dailyHistoryCount_; ++i) dailyHistory_[i - 1] = dailyHistory_[i];
    dailyHistoryCount_--;
  }
  DailyUsageRecord& record = dailyHistory_[dailyHistoryCount_++];
  strlcpy(record.date, date, sizeof(record.date));
  record.volumeM3 = volumeM3 > 0.0f ? volumeM3 : 0.0f;
  record.energyKwh = energyKwh > 0.0f ? energyKwh : 0.0f;
  saveHistory();
}

void UsageTracker::updatePeriods(float volume, float energy, time_t now) {
  UsageLogic::PeriodKeys keys;
  if (!UsageLogic::makePeriodKeys(now, keys)) return;
  bool changed = false;

  if (dayBaseline_ < 0.0f || strcmp(dayKey_, keys.day) != 0) {
    if (dayBaseline_ >= 0.0f && dayEnergyBaseline_ >= 0.0f && dayKey_[0] != '\0') {
      const float completedVolume = previousVolume_ > dayBaseline_ ? previousVolume_ - dayBaseline_ : 0.0f;
      const float completedEnergy = cumulativeEnergyKwh_ > dayEnergyBaseline_ ? cumulativeEnergyKwh_ - dayEnergyBaseline_ : 0.0f;
      addDailyRecord(dayKey_, completedVolume, completedEnergy);
    }
    dayBaseline_ = volume;
    dayEnergyBaseline_ = energy;
    strlcpy(dayKey_, keys.day, sizeof(dayKey_));
    changed = true;
  } else if (dayEnergyBaseline_ < 0.0f) {
    dayEnergyBaseline_ = energy;
    changed = true;
  }

  if (monthBaseline_ < 0.0f || strcmp(monthKey_, keys.month) != 0) {
    monthBaseline_ = volume;
    monthEnergyBaseline_ = energy;
    strlcpy(monthKey_, keys.month, sizeof(monthKey_));
    changed = true;
  } else if (monthEnergyBaseline_ < 0.0f) {
    monthEnergyBaseline_ = energy;
    changed = true;
  }

  if (yearBaseline_ < 0.0f || strcmp(yearKey_, keys.year) != 0) {
    yearBaseline_ = volume;
    yearEnergyBaseline_ = energy;
    strlcpy(yearKey_, keys.year, sizeof(yearKey_));
    changed = true;
  } else if (yearEnergyBaseline_ < 0.0f) {
    yearEnergyBaseline_ = energy;
    changed = true;
  }
  if (changed) saveState();
}

void UsageTracker::updateContinuousFlow(float flowM3h) {
  const bool enabled = Config::continuousFlowThresholdM3h > 0.0f && Config::continuousFlowAlertMinutes > 0;
  if (!enabled) {
    continuousFlowStartedMs_ = 0;
    lastPositiveFlowMs_ = 0;
    snapshot_.continuousFlowMinutes = 0;
    snapshot_.continuousFlowAlert = false;
    return;
  }

  const uint32_t nowMs = millis();
  if (flowM3h >= Config::continuousFlowThresholdM3h) {
    if (continuousFlowStartedMs_ == 0) continuousFlowStartedMs_ = nowMs;
    lastPositiveFlowMs_ = nowMs;
  } else if (continuousFlowStartedMs_ != 0) {
    const uint32_t pollGrace = Config::pollIntervalMs > 200000UL ? Config::pollIntervalMs * 3UL : 600000UL;
    if (lastPositiveFlowMs_ == 0 || nowMs - lastPositiveFlowMs_ > pollGrace) {
      continuousFlowStartedMs_ = 0;
      lastPositiveFlowMs_ = 0;
    }
  }

  if (continuousFlowStartedMs_ == 0) {
    snapshot_.continuousFlowMinutes = 0;
    snapshot_.continuousFlowAlert = false;
    return;
  }
  snapshot_.continuousFlowMinutes = (nowMs - continuousFlowStartedMs_) / 60000UL;
  snapshot_.continuousFlowAlert = snapshot_.continuousFlowMinutes >= Config::continuousFlowAlertMinutes;
}

void UsageTracker::update(float rawVolumeM3, time_t now, bool timeSynchronized) {
  const float correctedVolume = rawVolumeM3 + Config::meterOffsetM3;
  const bool meterReset = previousVolume_ >= 0.0f && correctedVolume + 0.001f < previousVolume_;
  const float positiveDelta = meterReset ? 0.0f : CoreLogic::positiveDelta(previousVolume_, correctedVolume);
  const float energyFactor = Config::calorificValue * Config::correctionFactor;

  if (meterReset) Logger::warn("Meter value decreased; treating as replacement/reset");
  if (cumulativeEnergyKwh_ < 0.0f) cumulativeEnergyKwh_ = correctedVolume * energyFactor;
  else cumulativeEnergyKwh_ += positiveDelta * energyFactor;

  if (meterReset) {
    dayBaseline_ = monthBaseline_ = yearBaseline_ = correctedVolume;
    dayEnergyBaseline_ = monthEnergyBaseline_ = yearEnergyBaseline_ = cumulativeEnergyKwh_;
  }
  if (timeSynchronized && now > 0) updatePeriods(correctedVolume, cumulativeEnergyKwh_, now);

  snapshot_.rawVolumeM3 = rawVolumeM3;
  snapshot_.correctedVolumeM3 = correctedVolume;
  snapshot_.energyKwh = cumulativeEnergyKwh_;
  snapshot_.timestamp = timeSynchronized ? now : 0;
  snapshot_.timeSynchronized = timeSynchronized;

  const uint32_t currentReadingMs = millis();
  if (meterReset || flowReferenceVolume_ < 0.0f || flowReferenceMs_ == 0) {
    flowReferenceVolume_ = correctedVolume;
    flowReferenceMs_ = currentReadingMs;
    snapshot_.flowM3h = 0.0f;
  } else if (CoreLogic::positiveDelta(flowReferenceVolume_, correctedVolume) > 0.0f) {
    const uint32_t elapsedSeconds = (currentReadingMs - flowReferenceMs_) / 1000UL;
    snapshot_.flowM3h = CoreLogic::calculateFlowM3h(flowReferenceVolume_, correctedVolume, elapsedSeconds, Config::maxFlowM3h);
    flowReferenceVolume_ = correctedVolume;
    flowReferenceMs_ = currentReadingMs;
  } else {
    snapshot_.flowM3h = 0.0f;
  }
  updateContinuousFlow(snapshot_.flowM3h);

  if (timeSynchronized && dayBaseline_ >= 0.0f) {
    snapshot_.dayM3 = correctedVolume > dayBaseline_ ? correctedVolume - dayBaseline_ : 0.0f;
    snapshot_.monthM3 = correctedVolume > monthBaseline_ ? correctedVolume - monthBaseline_ : 0.0f;
    snapshot_.yearM3 = correctedVolume > yearBaseline_ ? correctedVolume - yearBaseline_ : 0.0f;
    snapshot_.dayEnergyKwh = cumulativeEnergyKwh_ > dayEnergyBaseline_ ? cumulativeEnergyKwh_ - dayEnergyBaseline_ : 0.0f;
    snapshot_.monthEnergyKwh = cumulativeEnergyKwh_ > monthEnergyBaseline_ ? cumulativeEnergyKwh_ - monthEnergyBaseline_ : 0.0f;
    snapshot_.yearEnergyKwh = cumulativeEnergyKwh_ > yearEnergyBaseline_ ? cumulativeEnergyKwh_ - yearEnergyBaseline_ : 0.0f;
  }

  previousVolume_ = correctedVolume;
  static uint32_t lastPersist = 0;
  if (millis() - lastPersist >= 3600000UL) {
    saveState();
    lastPersist = millis();
  }
}

const UsageSnapshot& UsageTracker::snapshot() { return snapshot_; }

const DailyUsageRecord* UsageTracker::dailyHistory(size_t& count) {
  count = dailyHistoryCount_;
  return dailyHistory_;
}
