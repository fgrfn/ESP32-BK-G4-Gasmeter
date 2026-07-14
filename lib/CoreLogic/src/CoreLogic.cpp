#include "CoreLogic.h"
#include <cctype>
#include <cmath>
#include <cstring>

namespace {
constexpr uint32_t kMinPollMs = 10000;
constexpr uint32_t kMaxPollMs = 3600000;
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
