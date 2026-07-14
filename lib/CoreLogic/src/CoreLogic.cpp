#include "CoreLogic.h"
#include <cctype>
#include <cmath>
#include <cstring>

namespace {
constexpr uint32_t kMinPollMs = 10000;
constexpr uint32_t kMaxPollMs = 3600000;

bool isLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}
}

uint32_t CoreLogic::clampPollIntervalMs(uint32_t value) {
  if (value < kMinPollMs) return kMinPollMs;
  if (value > kMaxPollMs) return kMaxPollMs;
  return value;
}

float CoreLogic::calculateFlowM3h(float previousVolume, float currentVolume, uint32_t elapsedSeconds, float maxFlowM3h) {
  if (!std::isfinite(previousVolume) || !std::isfinite(currentVolume) || elapsedSeconds == 0) return 0.0f;
  const float delta = currentVolume - previousVolume;
  if (delta < 0.0f) return 0.0f;
  const float flow = delta * 3600.0f / static_cast<float>(elapsedSeconds);
  if (!std::isfinite(flow) || flow < 0.0f || flow > maxFlowM3h) return 0.0f;
  return flow;
}

bool CoreLogic::isValidHostname(const char* value) {
  if (!value) return false;
  const size_t len = std::strlen(value);
  if (len == 0 || len > 63 || value[0] == '-' || value[len - 1] == '-') return false;
  for (size_t i = 0; i < len; ++i) {
    const unsigned char c = static_cast<unsigned char>(value[i]);
    if (!(std::isalnum(c) || c == '-')) return false;
  }
  return true;
}

bool CoreLogic::isValidIsoDate(const char* value) {
  if (!value || std::strlen(value) != 10 || value[4] != '-' || value[7] != '-') return false;
  for (size_t i = 0; i < 10; ++i) {
    if (i == 4 || i == 7) continue;
    if (!std::isdigit(static_cast<unsigned char>(value[i]))) return false;
  }

  const int year = (value[0] - '0') * 1000 + (value[1] - '0') * 100 + (value[2] - '0') * 10 + (value[3] - '0');
  const int month = (value[5] - '0') * 10 + (value[6] - '0');
  const int day = (value[8] - '0') * 10 + (value[9] - '0');
  if (year < 1970 || month < 1 || month > 12 || day < 1) return false;

  static constexpr uint8_t daysPerMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int maxDay = daysPerMonth[month - 1];
  if (month == 2 && isLeapYear(year)) maxDay = 29;
  return day <= maxDay;
}

void CoreLogic::makeSafeId(const char* input, char* output, size_t outputSize) {
  if (!output || outputSize == 0) return;
  size_t written = 0;
  bool lastUnderscore = false;
  for (size_t i = 0; input && input[i] && written + 1 < outputSize; ++i) {
    const unsigned char c = static_cast<unsigned char>(input[i]);
    if (std::isalnum(c)) {
      output[written++] = static_cast<char>(std::tolower(c));
      lastUnderscore = false;
    } else if (!lastUnderscore && written > 0) {
      output[written++] = '_';
      lastUnderscore = true;
    }
  }
  while (written > 0 && output[written - 1] == '_') --written;
  output[written] = '\0';
}
