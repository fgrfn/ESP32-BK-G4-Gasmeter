#pragma once

#include <cstddef>
#include <cstdint>

namespace CoreLogic {
uint32_t clampPollIntervalMs(uint32_t value);
uint32_t pollIntervalSecondsToMs(int64_t seconds);
float calculateFlowM3h(float previousVolume, float currentVolume, uint32_t elapsedSeconds, float maxFlowM3h);
float positiveDelta(float previousVolume, float currentVolume, float tolerance = 0.00001f);
bool isValidHostname(const char* value);
bool isValidIsoDate(const char* value);
bool isValidMqttBaseTopic(const char* value);
bool isValidTcpPort(int64_t value);
bool isSynchronizedEpoch(int64_t epochSeconds);
bool isFinite(float value);
bool isFiniteInRange(float value, float minimum, float maximum);
void makeSafeId(const char* input, char* output, size_t outputSize);
}
