#include "UsageLogic.h"
#include <cmath>
#include <cstring>

bool UsageLogic::makePeriodKeys(std::time_t timestamp, PeriodKeys& keys) {
  if (timestamp <= 0) return false;
  std::tm local = {};
#if defined(_WIN32)
  if (localtime_s(&local, &timestamp) != 0) return false;
#else
  if (!localtime_r(&timestamp, &local)) return false;
#endif
  return std::strftime(keys.day, sizeof(keys.day), "%Y-%m-%d", &local) > 0 &&
         std::strftime(keys.month, sizeof(keys.month), "%Y-%m", &local) > 0 &&
         std::strftime(keys.year, sizeof(keys.year), "%Y", &local) > 0;
}

float UsageLogic::normalizePersistedCounter(float value) {
  return std::isfinite(value) && value >= 0.0f ? value : -1.0f;
}

bool UsageLogic::isValidDailyRecord(float volumeM3, float energyKwh) {
  return std::isfinite(volumeM3) && volumeM3 >= 0.0f &&
         std::isfinite(energyKwh) && energyKwh >= 0.0f;
}
