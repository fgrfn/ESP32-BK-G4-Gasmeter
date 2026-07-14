#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>
#include "MBusProtocol.h"

static std::vector<uint8_t> validFrame() {
  std::vector<uint8_t> frame = {0x68, 0x09, 0x09, 0x68, 0x08, 0x00, 0x72, 0x0C, 0x13, 0x56, 0x34, 0x12, 0x00, 0x00, 0x16};
  uint8_t checksum = 0;
  for (size_t i = 4; i < frame.size() - 2; ++i) checksum = static_cast<uint8_t>(checksum + frame[i]);
  frame[frame.size() - 2] = checksum;
  return frame;
}

int main() {
  auto frame = validFrame();
  assert(MBusProtocol::expectedFrameLength(frame.data(), 4) == frame.size());
  auto result = MBusProtocol::parseVolume(frame.data(), frame.size());
  assert(result.valid);
  assert(result.error == MBusProtocol::Error::None);
  assert(std::fabs(result.volumeM3 - 123.456) < 0.0001);

  frame[frame.size() - 2] ^= 0x01;
  result = MBusProtocol::parseVolume(frame.data(), frame.size());
  assert(!result.valid);
  assert(result.error == MBusProtocol::Error::ChecksumMismatch);

  frame = validFrame();
  frame[9] = 0xFA;
  uint8_t checksum = 0;
  for (size_t i = 4; i < frame.size() - 2; ++i) checksum = static_cast<uint8_t>(checksum + frame[i]);
  frame[frame.size() - 2] = checksum;
  result = MBusProtocol::parseVolume(frame.data(), frame.size());
  assert(result.error == MBusProtocol::Error::InvalidBcd);
  return 0;
}
