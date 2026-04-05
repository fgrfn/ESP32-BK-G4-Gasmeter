#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <vector>
#include "constants.h"

// ==========================================
// LOG LEVELS
// ==========================================
enum LogLevel {
  LOG_ERROR = 0,
  LOG_WARN = 1,
  LOG_INFO = 2,
  LOG_DEBUG = 3
};

// ==========================================
// LOG ENTRY STRUCTURE
// ==========================================
struct LogEntry {
  unsigned long timestamp;
  String message;
  LogLevel level;
};

// ==========================================
// LOGGER CLASS
// ==========================================
class Logger {
public:
  static void init();
  static void addLog(const String& msg, LogLevel level = LOG_INFO);
  static void error(const String& msg);
  static void warn(const String& msg);
  static void info(const String& msg);
  static void debug(const String& msg);
  static void setTimeInitialized(bool initialized);
  static const std::vector<LogEntry>& getLogBuffer();
  static void clearLogs();
  static void setLogLevel(LogLevel level);
  static LogLevel getLogLevel();
  static String levelToString(LogLevel level);
  static String levelToIcon(LogLevel level);
  static String levelToColor(LogLevel level);

private:
  static std::vector<LogEntry> logBuffer;
  static bool timeInitialized;
  static LogLevel currentLogLevel;
  
  static String stripAnsiCodes(const String& msg);
};

#endif // LOGGER_H
