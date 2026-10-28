#pragma once

#include <cstddef>
#include <ctime>

namespace UsageLogic {
struct PeriodKeys {
  char day[11] = "";
  char month[8] = "";
  char year[5] = "";
};

bool makePeriodKeys(std::time_t timestamp, PeriodKeys& keys);
float normalizePersistedCounter(float value);
bool isValidDailyRecord(float volumeM3, float energyKwh);
}
