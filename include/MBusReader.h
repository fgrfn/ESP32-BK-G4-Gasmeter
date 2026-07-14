#pragma once

#include <Arduino.h>
#include <functional>
#include "MBusProtocol.h"
#include "constants.h"

struct MBusStats {
  uint32_t polls = 0;
  uint32_t successful = 0;
  uint32_t timeouts = 0;
  uint32_t parseErrors = 0;
  uint32_t checksumErrors = 0;
  uint32_t lastResponseMs = 0;
  uint32_t averageResponseMs = 0;
  uint32_t lastSuccessMs = 0;
  MBusProtocol::Error lastError = MBusProtocol::Error::None;
  String lastHexDump;
};

class MBusReader {
 public:
  using ReadingCallback = std::function<void(float rawVolumeM3)>;
  static void begin(ReadingCallback callback);
  static void loop();
  static bool trigger();
  static bool healthy();
  static const MBusStats& stats();

 private:
  enum class State : uint8_t { Idle, Waiting };
  static void sendPoll();
  static void finishFrame();
  static HardwareSerial serial_;
  static State state_;
  static uint8_t buffer_[Constants::MBUS_BUFFER_SIZE];
  static size_t length_;
  static size_t expectedLength_;
  static uint32_t sentAt_;
  static uint32_t nextPollAt_;
  static uint64_t totalResponseMs_;
  static MBusStats stats_;
  static ReadingCallback callback_;
};
