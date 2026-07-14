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
  assert(CoreLogic::isValidHostname("esp32-gasmeter"));
  assert(!CoreLogic::isValidHostname("bad host"));
  char id[32];
  CoreLogic::makeSafeId("Gas Meter #1", id, sizeof(id));
  assert(std::strcmp(id, "gas_meter_1") == 0);
  return 0;
}
