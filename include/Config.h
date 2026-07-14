#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "constants.h"

struct TariffPeriod {
  char validFrom[11] = "1970-01-01";
  float workPricePerKwh = 0.12f;
  float basePriceMonthly = 10.0f;
};

class Config {
 public:
  static void begin();
  static bool load();
  static bool save();
  static void factoryReset();
  static bool importJson(JsonVariantConst root, String& error);
  static void toJson(JsonObject root, bool includeSecrets);
  static bool validate(String& error);
  static const TariffPeriod& activeTariff(time_t now);
  static void generateDeviceIdentity();

  static char ssid[33];
  static char wifiPassword[65];
  static char hostname[64];
  static bool staticIpEnabled;
  static char staticIp[16];
  static char gateway[16];
  static char subnet[16];
  static char dns[16];

  static char mqttHost[65];
  static uint16_t mqttPort;
  static char mqttUser[65];
  static char mqttPassword[65];
  static char mqttBaseTopic[65];
  static bool mqttTls;
  static bool mqttTlsInsecure;
  static String mqttCaCert;
  static bool mqttCommandsEnabled;

  static char webUser[33];
  static char webPassword[65];
  static char otaPassword[65];
  static char deviceId[33];

  static uint32_t pollIntervalMs;
  static float calorificValue;
  static float correctionFactor;
  static float meterOffsetM3;
  static float maxFlowM3h;
  static char timezone[65];
  static TariffPeriod tariffs[4];
  static uint8_t tariffCount;

 private:
  static Preferences preferences_;
  static void setDefaults();
  static void migrate(uint32_t fromSchema);
  static void ensureSecrets();
  static bool validIpv4(const char* value);
};
