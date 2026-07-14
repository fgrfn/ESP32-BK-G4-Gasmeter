#pragma once

#include <cstddef>
#include <cstdint>

namespace CoreLogic {
uint32_t clampPollIntervalMs(uint32_t value);
float calculateFlowM3h(float previousVolume, float currentVolume, uint32_t elapsedSeconds, float maxFlowM3h);
bool isValidHostname(const char* value);
bool isValidIsoDate(const char* value);
void makeSafeId(const char* input, char* output, size_t outputSize);
}
