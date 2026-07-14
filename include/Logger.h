#pragma once

#include <Arduino.h>
#include <vector>
#include "constants.h"

struct LogEntry {
  uint32_t timestampMs;
  String message;
};

class Logger {
 public:
  static void begin();
  static void info(const String& message);
  static void warn(const String& message);
  static void error(const String& message);
  static const std::vector<LogEntry>& entries();
  static void clear();

 private:
  static void add(const char* level, const String& message);
  static std::vector<LogEntry> buffer_;
};
