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

static std::vector<uint8_t> validFrame() {
  std::vector<uint8_t> frame = {0x68, 0x09, 0x09, 0x68, 0x08, 0x00, 0x72, 0x0C, 0x13, 0x56, 0x34, 0x12, 0x00, 0x00, 0x16};
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
  return 0;
}
