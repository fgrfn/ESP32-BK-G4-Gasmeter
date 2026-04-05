#ifndef MBUS_READER_H
#define MBUS_READER_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include "constants.h"

// ==========================================
// M-BUS STATISTICS
// ==========================================
struct MBusStats {
  unsigned long totalPolls = 0;
  unsigned long successfulPolls = 0;
  unsigned long totalResponseTime = 0;
  unsigned long lastResponseTime = 0;
  unsigned long avgResponseTime = 0;
  String lastHexDump = "";
};

// ==========================================
// M-BUS STATE MACHINE
// ==========================================
enum MBusState { 
  MBUS_IDLE, 
  MBUS_WAIT_RESPONSE 
};

// ==========================================
// M-BUS READER CLASS
// ==========================================
class MBusReader {
public:
  static void init();
  static void poll();
  static void processResponse(float& volume, bool& success);
  static float parseGasVolumeBCD(const uint8_t* data, size_t len);
  
  static MBusStats& getStats() { return stats; }
  static MBusState getState() { return state; }
  static unsigned long getLastActionTime() { return lastActionTime; }
  static void resetStats();
  
private:
  static HardwareSerial serial;
  static MBusState state;
  static unsigned long lastActionTime;
  static uint8_t buffer[MBUS_BUFFER_SIZE];
  static size_t bufferLen;
  static MBusStats stats;
  
  static void sendPollFrame();
  static void readResponse();
  static String createHexDump();
};

#endif // MBUS_READER_H
