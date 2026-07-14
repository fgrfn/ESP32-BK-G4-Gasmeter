#include "MBusReader.h"
#include <cstring>
#include "Config.h"
#include "Logger.h"

HardwareSerial MBusReader::serial_(1);
MBusReader::State MBusReader::state_ = MBusReader::State::Idle;
uint8_t MBusReader::buffer_[Constants::MBUS_BUFFER_SIZE] = {};
size_t MBusReader::length_ = 0;
size_t MBusReader::expectedLength_ = 0;
uint32_t MBusReader::sentAt_ = 0;
uint32_t MBusReader::nextPollAt_ = 0;
uint64_t MBusReader::totalResponseMs_ = 0;
MBusStats MBusReader::stats_;
MBusReader::ReadingCallback MBusReader::callback_;

namespace {
void resyncLongFrame(uint8_t* buffer, size_t& length) {
  size_t nextStart = 1;
  while (nextStart < length && buffer[nextStart] != 0x68) ++nextStart;
  if (nextStart >= length) {
    length = 0;
    return;
  }
  const size_t remaining = length - nextStart;
  std::memmove(buffer, buffer + nextStart, remaining);
  length = remaining;
}
}

void MBusReader::begin(ReadingCallback callback) {
  callback_ = callback;
  serial_.begin(Constants::MBUS_BAUD, SERIAL_8E1, Constants::MBUS_RX_PIN, Constants::MBUS_TX_PIN);
  nextPollAt_ = millis() + 1000;
  Logger::info("M-Bus UART initialized");
}

void MBusReader::sendPoll() {
  static const uint8_t frame[] = {0x10, 0x5B, 0x00, 0x5B, 0x16};
  while (serial_.available()) serial_.read();
  serial_.write(frame, sizeof(frame));
  serial_.flush();
  length_ = 0;
  expectedLength_ = 0;
  sentAt_ = millis();
  state_ = State::Waiting;
  stats_.polls++;
}

bool MBusReader::trigger() {
  if (state_ != State::Idle) return false;
  sendPoll();
  return true;
}

void MBusReader::finishFrame() {
  stats_.lastResponseMs = millis() - sentAt_;
  totalResponseMs_ += stats_.lastResponseMs;
  stats_.lastHexDump = "";
  const size_t dumpLength = length_ < 64 ? length_ : 64;
  for (size_t i = 0; i < dumpLength; ++i) {
    char part[4];
    snprintf(part, sizeof(part), "%02X ", buffer_[i]);
    stats_.lastHexDump += part;
  }

  const MBusProtocol::Result result = MBusProtocol::parseVolume(buffer_, length_);
  stats_.lastError = result.error;
  if (result.valid) {
    stats_.successful++;
    Logger::info("M-Bus volume: " + String(result.volumeM3, 3) + " m3");
    if (callback_) callback_(static_cast<float>(result.volumeM3));
  } else {
    stats_.parseErrors++;
    if (result.error == MBusProtocol::Error::ChecksumMismatch) stats_.checksumErrors++;
    Logger::error("M-Bus parse error: " + String(MBusProtocol::errorToString(result.error)));
  }
  const uint32_t completedResponses = stats_.successful + stats_.parseErrors;
  stats_.averageResponseMs = completedResponses ? static_cast<uint32_t>(totalResponseMs_ / completedResponses) : 0;
  state_ = State::Idle;
  nextPollAt_ = millis() + Config::pollIntervalMs;
}

void MBusReader::loop() {
  const uint32_t now = millis();
  if (state_ == State::Idle) {
    if (static_cast<int32_t>(now - nextPollAt_) >= 0) sendPoll();
    return;
  }

  while (serial_.available()) {
    const uint8_t byte = static_cast<uint8_t>(serial_.read());
    if (length_ == 0) {
      // Ignore request echo, ACK and line noise until a long-frame start byte arrives.
      if (byte != 0x68) continue;
      buffer_[length_++] = byte;
    } else {
      if (length_ >= sizeof(buffer_)) {
        length_ = 0;
        expectedLength_ = 0;
        continue;
      }
      buffer_[length_++] = byte;
    }

    if (expectedLength_ == 0 && length_ >= 4) {
      expectedLength_ = MBusProtocol::expectedFrameLength(buffer_, length_);
      if (expectedLength_ == 0 || expectedLength_ > sizeof(buffer_)) {
        resyncLongFrame(buffer_, length_);
        expectedLength_ = length_ >= 4 ? MBusProtocol::expectedFrameLength(buffer_, length_) : 0;
        if (expectedLength_ > sizeof(buffer_)) {
          length_ = 0;
          expectedLength_ = 0;
        }
        continue;
      }
    }

    if (expectedLength_ > 0 && length_ >= expectedLength_) break;
  }

  if (expectedLength_ > 0 && length_ >= expectedLength_) {
    finishFrame();
    return;
  }
  if (now - sentAt_ >= Constants::MBUS_RESPONSE_TIMEOUT_MS) {
    stats_.lastResponseMs = now - sentAt_;
    stats_.timeouts++;
    stats_.lastError = MBusProtocol::Error::Empty;
    Logger::error("M-Bus timeout");
    length_ = 0;
    expectedLength_ = 0;
    state_ = State::Idle;
    nextPollAt_ = now + Config::pollIntervalMs;
  }
}

const MBusStats& MBusReader::stats() { return stats_; }
