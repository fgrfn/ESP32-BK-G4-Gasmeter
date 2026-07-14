#include <cassert>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include "UsageLogic.h"

static std::time_t utc(int year, int month, int day, int hour, int minute) {
  std::tm value = {};
  value.tm_year = year - 1900;
  value.tm_mon = month - 1;
  value.tm_mday = day;
  value.tm_hour = hour;
  value.tm_min = minute;
  return timegm(&value);
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();

  UsageLogic::PeriodKeys keys;
  assert(!UsageLogic::makePeriodKeys(0, keys));

  assert(UsageLogic::makePeriodKeys(utc(2026, 3, 29, 0, 30), keys));
  assert(std::strcmp(keys.day, "2026-03-29") == 0);
  assert(std::strcmp(keys.month, "2026-03") == 0);

  assert(UsageLogic::makePeriodKeys(utc(2026, 3, 29, 1, 30), keys));
  assert(std::strcmp(keys.day, "2026-03-29") == 0);

  assert(UsageLogic::makePeriodKeys(utc(2026, 3, 31, 22, 30), keys));
  assert(std::strcmp(keys.day, "2026-04-01") == 0);
  assert(std::strcmp(keys.month, "2026-04") == 0);

  assert(UsageLogic::makePeriodKeys(utc(2026, 12, 31, 23, 30), keys));
  assert(std::strcmp(keys.day, "2027-01-01") == 0);
  assert(std::strcmp(keys.year, "2027") == 0);
  return 0;
}
