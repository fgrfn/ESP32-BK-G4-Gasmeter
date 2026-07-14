#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>
#include "MBusProtocol.h"

static void updateChecksum(std::vector<uint8_t>& frame) {
  uint8_t checksum = 0;
  for (size_t i = 4; i < frame.size() - 2; ++i) checksum = static_cast<uint8_t>(checksum + frame[i]);
  frame[frame.size() - 2] = checksum;
}

static std::vector<uint8_t> validFrame(uint32_t bcdValue = 123456) {
  std::vector<uint8_t> frame = {0x68, 0x09, 0x09, 0x68, 0x08, 0x00, 0x72, 0x0C, 0x13, 0, 0, 0, 0, 0, 0x16};
  for (size_t i = 0; i < 4; ++i) {
    const uint8_t low = bcdValue % 10;
    bcdValue /= 10;
    const uint8_t high = bcdValue % 10;
    bcdValue /= 10;
    frame[9 + i] = static_cast<uint8_t>(low | (high << 4));
  }
  updateChecksum(frame);
  return frame;
}

int main() {
  const uint8_t ack[] = {0xE5};
  const uint8_t shortFrame[] = {0x10, 0x5B, 0x00, 0x5B, 0x16};
  assert(MBusProtocol::expectedFrameLength(ack, sizeof(ack)) == 1);
  assert(MBusProtocol::expectedFrameLength(shortFrame, sizeof(shortFrame)) == 5);

  auto frame = validFrame();
  assert(MBusProtocol::expectedFrameLength(frame.data(), 4) == frame.size());
  auto result = MBusProtocol::parseVolume(frame.data(), frame.size());
  assert(result.valid);
  assert(result.error == MBusProtocol::Error::None);
  assert(std::fabs(result.volumeM3 - 123.456) < 0.0001);

  frame = validFrame(1);
  result = MBusProtocol::parseVolume(frame.data(), frame.size());
  assert(result.valid);
  assert(std::fabs(result.volumeM3 - 0.001) < 0.0001);

  frame = validFrame(99999999);
  result = MBusProtocol::parseVolume(frame.data(), frame.size());
  assert(result.valid);
  assert(std::fabs(result.volumeM3 - 99999.999) < 0.001);

  frame[frame.size() - 2] ^= 0x01;
  result = MBusProtocol::parseVolume(frame.data(), frame.size());
  assert(!result.valid);
  assert(result.error == MBusProtocol::Error::ChecksumMismatch);

  frame = validFrame();
  frame[9] = 0xFA;
  updateChecksum(frame);
  result = MBusProtocol::parseVolume(frame.data(), frame.size());
  assert(!result.valid);
  assert(result.error == MBusProtocol::Error::InvalidBcd);

  frame = validFrame();
  frame.back() = 0x00;
  result = MBusProtocol::parseVolume(frame.data(), frame.size());
  assert(!result.valid);
  assert(result.error == MBusProtocol::Error::InvalidStop);

  frame = validFrame();
  frame.pop_back();
  result = MBusProtocol::parseVolume(frame.data(), frame.size());
  assert(!result.valid);
  assert(result.error == MBusProtocol::Error::LengthMismatch);

  frame = validFrame();
  frame[7] = 0x01;
  updateChecksum(frame);
  result = MBusProtocol::parseVolume(frame.data(), frame.size());
  assert(!result.valid);
  assert(result.error == MBusProtocol::Error::VolumeRecordNotFound);

  frame = validFrame();
  frame[2] = 0x08;
  result = MBusProtocol::parseVolume(frame.data(), frame.size());
  assert(!result.valid);
  assert(result.error == MBusProtocol::Error::UnsupportedFrame);
  assert(MBusProtocol::expectedFrameLength(frame.data(), 4) == 0);

  result = MBusProtocol::parseVolume(nullptr, 0);
  assert(!result.valid);
  assert(result.error == MBusProtocol::Error::Empty);

  uint32_t random = 0xC0FFEEu;
  std::vector<uint8_t> fuzz(256);
  for (size_t iteration = 0; iteration < 10000; ++iteration) {
    random = random * 1664525u + 1013904223u;
    const size_t length = random % fuzz.size();
    for (size_t i = 0; i < length; ++i) {
      random = random * 1664525u + 1013904223u;
      fuzz[i] = static_cast<uint8_t>(random >> 24);
    }
    const auto fuzzResult = MBusProtocol::parseVolume(fuzz.data(), length);
    if (fuzzResult.valid) assert(fuzzResult.error == MBusProtocol::Error::None);
  }
  return 0;
}
