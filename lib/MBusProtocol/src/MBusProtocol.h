#pragma once

#include <cstddef>
#include <cstdint>

namespace MBusProtocol {
enum class Error : uint8_t {
  None,
  Empty,
  UnsupportedFrame,
  LengthMismatch,
  InvalidStop,
  ChecksumMismatch,
  InvalidBcd,
  VolumeRecordNotFound
};

struct Result {
  bool valid = false;
  double volumeM3 = 0.0;
  Error error = Error::Empty;
  uint8_t expectedChecksum = 0;
  uint8_t receivedChecksum = 0;
  size_t frameLength = 0;
};

size_t expectedFrameLength(const uint8_t* data, size_t len);
Result parseVolume(const uint8_t* data, size_t len);
const char* errorToString(Error error);
}
