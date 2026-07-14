#include "Logger.h"

std::vector<LogEntry> Logger::buffer_;

void Logger::begin() {
  buffer_.reserve(Constants::MAX_LOG_ENTRIES);
}

void Logger::add(const char* level, const String& message) {
  String line;
  line.reserve(message.length() + 10);
  line += '[';
  line += level;
  line += "] ";
  line += message;
  Serial.printf("[%lus] %s\n", millis() / 1000UL, line.c_str());
  if (buffer_.size() >= Constants::MAX_LOG_ENTRIES) buffer_.erase(buffer_.begin());
  buffer_.push_back({millis(), line});
}

void Logger::info(const String& message) { add("INFO", message); }
void Logger::warn(const String& message) { add("WARN", message); }
void Logger::error(const String& message) { add("ERROR", message); }
const std::vector<LogEntry>& Logger::entries() { return buffer_; }
void Logger::clear() { buffer_.clear(); }
