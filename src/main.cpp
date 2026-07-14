#include <Arduino.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <time.h>
#include "Config.h"
#include "Logger.h"
#include "MBusReader.h"
#include "MQTTHandler.h"
#include "UsageTracker.h"
#include "WebServerManager.h"
#include "WiFiManager.h"
#include "constants.h"

namespace {
Preferences systemPreferences;
bool safeMode = false;
bool stableBootRecorded = false;
uint32_t lastLedChange = 0;
bool ledState = false;

bool resetButtonHeld() {
  pinMode(Constants::RESET_BUTTON_PIN, INPUT_PULLUP);
  if (digitalRead(Constants::RESET_BUTTON_PIN) != LOW) return false;
  const uint32_t started = millis();
  while (digitalRead(Constants::RESET_BUTTON_PIN) == LOW && millis() - started < 3000) delay(20);
  return millis() - started >= 3000;
}

void initializeBootGuard() {
  systemPreferences.begin("system", false);
  uint8_t failures = systemPreferences.getUChar("boot_failures", 0);
  failures = failures == 255 ? 255 : static_cast<uint8_t>(failures + 1);
  systemPreferences.putUChar("boot_failures", failures);
  systemPreferences.end();
  safeMode = failures >= Constants::SAFE_MODE_BOOT_THRESHOLD;
  if (safeMode) Logger::warn("Safe mode enabled after repeated unstable boots");
}

void recordStableBoot() {
  if (stableBootRecorded || millis() < Constants::STABLE_BOOT_AFTER_MS) return;
  stableBootRecorded = true;
  systemPreferences.begin("system", false);
  systemPreferences.putUChar("boot_failures", 0);
  systemPreferences.end();
  esp_ota_mark_app_valid_cancel_rollback();
  Logger::info("Boot marked stable and OTA image accepted");
}

void updateLed() {
  uint32_t interval = 2000;
  if (safeMode || WiFiManager::accessPointMode()) interval = 150;
  else if (!WiFiManager::connected()) interval = 250;
  else if (!MQTTHandler::connected() && Config::mqttHost[0]) interval = 600;
  if (millis() - lastLedChange >= interval) {
    ledState = !ledState;
    digitalWrite(Constants::STATUS_LED_PIN, ledState);
    lastLedChange = millis();
  }
}

void initializeOta() {
  if (!WiFiManager::connected()) return;
  ArduinoOTA.setHostname(Config::hostname);
  ArduinoOTA.setPassword(Config::otaPassword);
  ArduinoOTA.onStart([] { Logger::warn("ArduinoOTA started"); });
  ArduinoOTA.onEnd([] { Logger::info("ArduinoOTA completed"); });
  ArduinoOTA.onError([](ota_error_t error) { Logger::error("ArduinoOTA error " + String(error)); });
  ArduinoOTA.begin();
}

void initializeMdns() {
  if (!WiFiManager::connected()) return;
  if (MDNS.begin(Config::hostname)) MDNS.addService("http", "tcp", 80);
  else Logger::warn("mDNS initialization failed");
}

void initializeTime() {
  if (!WiFiManager::connected()) return;
  configTzTime(Config::timezone, Constants::NTP_SERVER_1, Constants::NTP_SERVER_2);
}

void handleReading(float rawVolumeM3) {
  time_t now = time(nullptr);
  if (now < 1577836800) now = 1704067200 + static_cast<time_t>(millis() / 1000UL);
  UsageTracker::update(rawVolumeM3, now);
  MQTTHandler::publishReading(UsageTracker::snapshot());
}
}

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(Constants::STATUS_LED_PIN, OUTPUT);
  digitalWrite(Constants::STATUS_LED_PIN, LOW);
  Logger::begin();
  Logger::info("ESP32 Gas Meter firmware " FIRMWARE_VERSION);

  Config::begin();
  Config::load();
  if (resetButtonHeld()) {
    Logger::warn("Physical factory reset requested");
    Config::factoryReset();
    Config::begin();
    Config::save();
  }
  initializeBootGuard();

  WiFiManager::begin(safeMode);
  initializeTime();
  initializeMdns();
  initializeOta();
  UsageTracker::begin();
  MBusReader::begin(handleReading);
  MQTTHandler::begin();
  WebServerManager::begin(safeMode);

  Serial.printf("Device ID: %s\nWeb login: %s / %s\n", Config::deviceId, Config::webUser, Config::webPassword);
  Logger::info("Setup complete");
}

void loop() {
  WiFiManager::loop();
  WebServerManager::loop();
  ArduinoOTA.handle();
  if (!safeMode) {
    MQTTHandler::loop();
    MBusReader::loop();
  }
  recordStableBoot();
  updateLed();
  delay(2);
}
