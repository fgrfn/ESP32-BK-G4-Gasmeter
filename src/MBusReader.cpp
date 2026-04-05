#include "MBusReader.h"
#include "Logger.h"

// Static member initialization
HardwareSerial MBusReader::serial(1);
MBusState MBusReader::state = MBUS_IDLE;
unsigned long MBusReader::lastActionTime = 0;
uint8_t MBusReader::buffer[MBUS_BUFFER_SIZE];
size_t MBusReader::bufferLen = 0;
MBusStats MBusReader::stats;

void MBusReader::init() {
  serial.begin(MBUS_BAUD, SERIAL_8E1, MBUS_RX_PIN, MBUS_TX_PIN);
  Serial.println("M-Bus UART bereit");
  state = MBUS_IDLE;
  bufferLen = 0;
  resetStats();
}

void MBusReader::resetStats() {
  stats.totalPolls = 0;
  stats.successfulPolls = 0;
  stats.totalResponseTime = 0;
  stats.lastResponseTime = 0;
  stats.avgResponseTime = 0;
  stats.lastHexDump = "";
}

void MBusReader::sendPollFrame() {
  uint8_t pollFrame[5] = {0x10, 0x5B, 0x00, 0x5B, 0x16};
  serial.write(pollFrame, sizeof(pollFrame));
  serial.flush();
  bufferLen = 0;
  lastActionTime = millis();
  state = MBUS_WAIT_RESPONSE;
  Serial.println("MBUS Poll gesendet, warte auf Antwort...");
  Logger::addLog("M-Bus: Poll gestartet");
}

void MBusReader::readResponse() {
  while (serial.available() && bufferLen < MBUS_BUFFER_SIZE) {
    buffer[bufferLen++] = serial.read();
  }
}

String MBusReader::createHexDump() {
  String hexDump = "";
  for (size_t i = 0; i < min(bufferLen, (size_t)32); i++) {
    char hex[4];
    sprintf(hex, "%02X ", buffer[i]);
    hexDump += hex;
  }
  if (bufferLen > 32) hexDump += "...";
  return hexDump;
}

void MBusReader::poll() {
  unsigned long now = millis();
  
  // Nur im IDLE-State pollen
  if (state == MBUS_IDLE) {
    sendPollFrame();
  }
}

void MBusReader::processResponse(float& volume, bool& success) {
  unsigned long now = millis();
  success = false;
  volume = -1.0;
  
  if (state != MBUS_WAIT_RESPONSE) {
    return;
  }
  
  // Response lesen
  readResponse();
  
  // Timeout oder Buffer voll?
  if ((now - lastActionTime >= MBUS_RESPONSE_TIMEOUT) || bufferLen >= MBUS_BUFFER_SIZE) {
    stats.totalPolls++;
    stats.lastResponseTime = now - lastActionTime;
    stats.totalResponseTime += stats.lastResponseTime;
    
    if (bufferLen > 0) {
      char msg[80];
      snprintf(msg, sizeof(msg), "M-Bus: Antwort erhalten (%u Bytes, %lums)", 
               bufferLen, stats.lastResponseTime);
      Serial.println(msg);
      Logger::addLog(msg);
      
      // Hex Dump speichern
      stats.lastHexDump = createHexDump();
      String hexLog = stats.lastHexDump;
      Logger::addLog("M-Bus: Rohdaten - " + hexLog);
      
      // Volume parsen
      volume = parseGasVolumeBCD(buffer, bufferLen);
      
      if (volume >= 0) {
        stats.successfulPolls++;
        stats.avgResponseTime = stats.totalPolls > 0 ? 
          (stats.totalResponseTime / stats.totalPolls) : 0;
        success = true;
      } else {
        Serial.println("Kein Volumenwert gefunden!");
      }
    } else {
      Serial.println("Keine MBUS Antwort erhalten");
    }
    
    state = MBUS_IDLE;
  }
}

float MBusReader::parseGasVolumeBCD(const uint8_t* data, size_t len) {
  for (size_t i = 0; i + 5 < len; i++) {
    if (data[i] == 0x0C && data[i+1] == 0x13) { // DIF=0x0C, VIF=0x13
      uint32_t value = 0;
      uint32_t factor = 1;
      for (int b = 0; b < 4; b++) {
        uint8_t byte = data[i+2+b];
        uint8_t lsn = byte & 0x0F;
        uint8_t msn = (byte >> 4) & 0x0F;
        value += lsn * factor; factor *= 10;
        value += msn * factor; factor *= 10;
      }
      return value / 1000.0; // 2 Dezimalstellen m³
    }
  }
  return -1; // nicht gefunden
}
