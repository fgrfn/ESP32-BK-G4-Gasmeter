#include "Logger.h"
#include <time.h>

// Static member initialization
std::vector<LogEntry> Logger::logBuffer;
bool Logger::timeInitialized = false;
LogLevel Logger::currentLogLevel = LOG_INFO; // Default: INFO

void Logger::init() {
  logBuffer.clear();
  logBuffer.reserve(MAX_LOG_ENTRIES);
  timeInitialized = false;
  currentLogLevel = LOG_INFO;
}

void Logger::addLog(const String& msg, LogLevel level) {
  // Filter based on log level
  if (level > currentLogLevel) {
    return; // Skip this log entry
  }
  
  // ANSI-Codes aus der Nachricht entfernen für WebUI (nur im logBuffer)
  String cleanMsg = stripAnsiCodes(msg);
  
  LogEntry entry;
  entry.timestamp = millis();
  entry.message = cleanMsg;
  entry.level = level;
  logBuffer.push_back(entry);
  
  // Ringbuffer: alte Einträge löschen
  if (logBuffer.size() > MAX_LOG_ENTRIES) {
    logBuffer.erase(logBuffer.begin());
  }
  
  // Serial Output mit echter Zeit wenn verfügbar (mit ANSI-Codes + Level)
  String levelStr = "[" + levelToString(level) + "] ";
  if (timeInitialized) {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
    Serial.print("[");
    Serial.print(timeStr);
    Serial.print("] ");
    Serial.print(levelStr);
    Serial.println(msg);
  } else {
    Serial.print("[");
    Serial.print(millis() / 1000);
    Serial.print("s] ");
    Serial.print(levelStr);
    Serial.println(msg);
  }
}

void Logger::error(const String& msg) {
  addLog(msg, LOG_ERROR);
}

void Logger::warn(const String& msg) {
  addLog(msg, LOG_WARN);
}

void Logger::info(const String& msg) {
  addLog(msg, LOG_INFO);
}

void Logger::debug(const String& msg) {
  addLog(msg, LOG_DEBUG);
}

void Logger::setLogLevel(LogLevel level) {
  currentLogLevel = level;
  Serial.println("Log Level set to: " + levelToString(level));
}

LogLevel Logger::getLogLevel() {
  return currentLogLevel;
}

String Logger::levelToString(LogLevel level) {
  switch(level) {
    case LOG_ERROR: return "ERROR";
    case LOG_WARN:  return "WARN";
    case LOG_INFO:  return "INFO";
    case LOG_DEBUG: return "DEBUG";
    default:        return "UNKNOWN";
  }
}

String Logger::levelToIcon(LogLevel level) {
  switch(level) {
    case LOG_ERROR: return "❌";
    case LOG_WARN:  return "⚠️";
    case LOG_INFO:  return "ℹ️";
    case LOG_DEBUG: return "🔍";
    default:        return "📝";
  }
}

String Logger::levelToColor(LogLevel level) {
  switch(level) {
    case LOG_ERROR: return "#ef4444";
    case LOG_WARN:  return "#fbbf24";
    case LOG_INFO:  return "#10b981";
    case LOG_DEBUG: return "#6366f1";
    default:        return "#9ca3af";
  }
}

void Logger::setTimeInitialized(bool initialized) {
  timeInitialized = initialized;
}

const std::vector<LogEntry>& Logger::getLogBuffer() {
  return logBuffer;
}

void Logger::clearLogs() {
  logBuffer.clear();
}

String Logger::stripAnsiCodes(const String& msg) {
  String cleanMsg = msg;
  cleanMsg.replace("\033[0m", "");   // ANSI_RESET
  cleanMsg.replace("\033[1m", "");   // ANSI_BOLD
  cleanMsg.replace("\033[31m", "");  // ANSI_RED
  cleanMsg.replace("\033[32m", "");  // ANSI_GREEN
  cleanMsg.replace("\033[33m", "");  // ANSI_YELLOW
  cleanMsg.replace("\033[34m", "");  // ANSI_BLUE
  cleanMsg.replace("\033[35m", "");  // ANSI_MAGENTA
  cleanMsg.replace("\033[36m", "");  // ANSI_CYAN
  cleanMsg.replace("\033[37m", "");  // ANSI_WHITE
  return cleanMsg;
}
