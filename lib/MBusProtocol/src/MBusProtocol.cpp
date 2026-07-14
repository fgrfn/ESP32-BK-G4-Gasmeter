#include "MBusProtocol.h"

namespace {
bool decodeBcd4(const uint8_t* data, uint32_t& value) {
  value = 0;
  uint32_t factor = 1;
  for (int i = 0; i < 4; ++i) {
    const uint8_t low = data[i] & 0x0F;
    const uint8_t high = (data[i] >> 4) & 0x0F;
    if (low > 9 || high > 9) return false;
    value += low * factor;
    factor *= 10;
    value += high * factor;
    factor *= 10;
  }
  return true;
}
}

size_t MBusProtocol::expectedFrameLength(const uint8_t* data, size_t len) {
  if (!data || len == 0) return 0;
  if (data[0] == 0xE5) return 1;
  if (data[0] == 0x10) return 5;
  if (data[0] == 0x68 && len >= 4 && data[3] == 0x68 && data[1] == data[2]) {
    return static_cast<size_t>(data[1]) + 6U;
  }
  return 0;
}

MBusProtocol::Result MBusProtocol::parseVolume(const uint8_t* data, size_t len) {
  Result result;
  result.frameLength = len;
  if (!data || len == 0) {
    result.error = Error::Empty;
    return result;
  }
  if (data[0] != 0x68 || len < 9 || data[3] != 0x68 || data[1] != data[2]) {
    result.error = Error::UnsupportedFrame;
    return result;
  }

  const size_t expected = static_cast<size_t>(data[1]) + 6U;
  if (len != expected) {
    result.error = Error::LengthMismatch;
    return result;
  }
  if (data[len - 1] != 0x16) {
    result.error = Error::InvalidStop;
    return result;
  }

  uint8_t checksum = 0;
  for (size_t i = 4; i < len - 2; ++i) checksum = static_cast<uint8_t>(checksum + data[i]);
  result.expectedChecksum = checksum;
  result.receivedChecksum = data[len - 2];
  if (checksum != result.receivedChecksum) {
    result.error = Error::ChecksumMismatch;
    return result;
  }

  // The BK-G4 encoder exposes total volume as DIF 0x0C (8-digit BCD)
  // followed by VIF 0x13 (volume in 10^-3 m3).
  for (size_t i = 4; i + 5 < len - 2; ++i) {
    if (data[i] != 0x0C || data[i + 1] != 0x13) continue;
    uint32_t raw = 0;
    if (!decodeBcd4(data + i + 2, raw)) {
      result.error = Error::InvalidBcd;
      return result;
    }
    result.volumeM3 = static_cast<double>(raw) / 1000.0;
    result.valid = true;
    result.error = Error::None;
    return result;
  }

  result.error = Error::VolumeRecordNotFound;
  return result;
}

const char* MBusProtocol::errorToString(Error error) {
  switch (error) {
    case Error::None: return "none";
    case Error::Empty: return "empty";
    case Error::UnsupportedFrame: return "unsupported_frame";
    case Error::LengthMismatch: return "length_mismatch";
    case Error::InvalidStop: return "invalid_stop";
    case Error::ChecksumMismatch: return "checksum_mismatch";
    case Error::InvalidBcd: return "invalid_bcd";
    case Error::VolumeRecordNotFound: return "volume_record_not_found";
  }
  return "unknown";
}
