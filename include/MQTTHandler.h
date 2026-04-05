#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include "constants.h"

// ==========================================
// ERROR STATISTICS
// ==========================================
struct ErrorStats {
  unsigned long mbusTimeouts = 0;
  unsigned long mbusParseErrors = 0;
  unsigned long mqttErrors = 0;
  unsigned long wifiDisconnects = 0;
  unsigned long lastError = 0;
  char lastErrorMsg[ERROR_MSG_MAX_LEN] = "";
};

// ==========================================
// MQTT HANDLER CLASS
// ==========================================
class MQTTHandler {
public:
  static void init(WiFiClient& wifiClient);
  static bool connect();
  static void loop();
  static bool isConnected();
  
  static bool publishVolume(float volume);
  static bool publishEnergy(float energy);
  static bool publishFlow(float flow);
  static bool publishBrennwert(float value);
  static bool publishZZahl(float value);
  static bool publishWiFiSignal(int rssi);
  static bool publishMBusRate(float rate);
  
  static void sendHomeAssistantDiscovery();
  static bool hasDiscoveryBeenSent() { return haDiscoverySent; }
  
  static ErrorStats& getErrorStats() { return errorStats; }
  static void resetErrorStats();
  static void logError(const char* msg);
  
private:
  static PubSubClient* client;
  static bool haDiscoverySent;
  static ErrorStats errorStats;
  
  static void sendDiscoverySensor(const char* name, const char* deviceClass, 
                                   const char* unit, const char* topic,
                                   const char* stateClass = nullptr);
  static void sendDiscoveryBinarySensor(const char* name, const char* deviceClass, 
                                        const char* topic);
};

#endif // MQTT_HANDLER_H
