#include <cassert>
#include <cmath>
#include <cstring>
#include "CoreLogic.h"

int main() {
  assert(CoreLogic::clampPollIntervalMs(1) == 10000);
  assert(CoreLogic::clampPollIntervalMs(30000) == 30000);
  assert(CoreLogic::clampPollIntervalMs(9999999) == 3600000);
  assert(std::fabs(CoreLogic::calculateFlowM3h(100.0f, 100.01f, 60, 25.0f) - 0.6f) < 0.001f);
  assert(CoreLogic::calculateFlowM3h(100.0f, 99.0f, 60, 25.0f) == 0.0f);
  assert(CoreLogic::calculateFlowM3h(100.0f, 101.0f, 1, 25.0f) == 0.0f);
  assert(std::fabs(CoreLogic::positiveDelta(10.0f, 10.5f) - 0.5f) < 0.0001f);
  assert(CoreLogic::positiveDelta(10.0f, 10.0005f) == 0.0f);
  assert(CoreLogic::positiveDelta(10.0f, 9.0f) == 0.0f);

  assert(CoreLogic::isValidHostname("esp32-gasmeter"));
  assert(!CoreLogic::isValidHostname("bad host"));
  assert(CoreLogic::isValidMqttBaseTopic("gasmeter/esp32_gas_aabbcc"));
  assert(!CoreLogic::isValidMqttBaseTopic("/gasmeter"));
  assert(!CoreLogic::isValidMqttBaseTopic("gasmeter/#"));
  assert(!CoreLogic::isValidMqttBaseTopic("gasmeter//device"));

  assert(CoreLogic::isValidIsoDate("1970-01-01"));
  assert(CoreLogic::isValidIsoDate("2024-02-29"));
  assert(!CoreLogic::isValidIsoDate("2023-02-29"));
  assert(!CoreLogic::isValidIsoDate("2026-13-01"));
  assert(!CoreLogic::isValidIsoDate("2026-00-10"));
  assert(!CoreLogic::isValidIsoDate("2026-04-31"));
  assert(!CoreLogic::isValidIsoDate("26-07-14"));
  assert(!CoreLogic::isSynchronizedEpoch(1577836799));
  assert(CoreLogic::isSynchronizedEpoch(1577836800));

  char id[32];
  CoreLogic::makeSafeId("Gas Meter #1", id, sizeof(id));
  assert(std::strcmp(id, "gas_meter_1") == 0);
  return 0;
}
