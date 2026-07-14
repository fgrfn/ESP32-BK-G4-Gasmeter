#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include "UsageTracker.h"

class MQTTHandler {
 public:
  static void begin();
  static void loop();
  static bool connected();
  static void publishReading(const UsageSnapshot& usage);
  static void publishDiagnostics();
  static uint32_t reconnectCount();
  static uint32_t publishErrors();

 private:
  static bool connect();
  static void configureTransport();
  static void sendDiscovery();
  static void sendSensorDiscovery(const char* objectId, const char* name, const char* stateTopic,
                                  const char* unit, const char* deviceClass, const char* stateClass,
                                  bool diagnostic = false);
  static void sendBinaryDiscovery(const char* objectId, const char* name, const char* stateTopic);
  static void sendNumberDiscovery(const char* objectId, const char* name, const char* stateTopic,
                                  const char* commandTopic, float minValue, float maxValue, float step,
                                  const char* unit);
  static void callback(char* topic, uint8_t* payload, unsigned int length);
  static String topic(const char* suffix);
  static WiFiClient plainClient_;
  static WiFiClientSecure secureClient_;
  static PubSubClient client_;
  static uint32_t nextReconnectAt_;
  static uint32_t reconnectDelayMs_;
  static uint32_t reconnectCount_;
  static uint32_t publishErrors_;
  static bool discoverySent_;
  static bool restartRequested_;
};
