#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include "BootGuard.h"
#include "Config.h"
#include "Logger.h"
#include "MBusReader.h"
#include "MQTTHandler.h"
#include "TimeManager.h"
#include "UsageTracker.h"
#include "WebServerManager.h"
#include "WiFiManager.h"
#include "constants.h"

namespace {
uint32_t lastLedChange = 0;
bool ledState = false;

bool resetButtonHeld() {
  pinMode(Constants::RESET_BUTTON_PIN, INPUT_PULLUP);
  if (digitalRead(Constants::RESET_BUTTON_PIN) != LOW) return false;
  const uint32_t started = millis();
  while (digitalRead(Constants::RESET_BUTTON_PIN) == LOW && millis() - started < 3000) delay(20);
  return millis() - started >= 3000;
}

void updateLed() {
  uint32_t interval = 2000;
  if (BootGuard::safeMode() || WiFiManager::accessPointMode()) interval = 150;
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
  if (Config::otaPassword[0] != '\0') {
    ArduinoOTA.setPassword(Config::otaPassword);
    Logger::info("ArduinoOTA password authentication enabled");
  } else {
    Logger::warn("ArduinoOTA has no password; restrict access to the trusted management network");
  }
  ArduinoOTA.onStart([] {
    BootGuard::markPlannedRestart("arduino_ota");
    Logger::warn("ArduinoOTA started");
  });
  ArduinoOTA.onEnd([] { Logger::info("ArduinoOTA completed"); });
  ArduinoOTA.onError([](ota_error_t error) { Logger::error("ArduinoOTA error " + String(error)); });
  ArduinoOTA.begin();
}

void initializeMdns() {
  if (!WiFiManager::connected()) return;
  if (MDNS.begin(Config::hostname)) MDNS.addService("http", "tcp", 80);
  else Logger::warn("mDNS initialization failed");
}

void handleReading(float rawVolumeM3) {
  UsageTracker::update(rawVolumeM3, TimeManager::now(), TimeManager::synchronized());
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

  BootGuard::begin();
  WiFiManager::begin(BootGuard::safeMode());
  TimeManager::begin();
  initializeMdns();
  initializeOta();
  UsageTracker::begin();
  MBusReader::begin(handleReading);
  MQTTHandler::begin();
  WebServerManager::begin(BootGuard::safeMode());

  Serial.printf("Device ID: %s\nWeb login: %s\n", Config::deviceId, Config::webAuthEnabled ? "enabled" : "disabled");
  if (Config::webAuthEnabled) Serial.printf("Web credentials: %s / %s\n", Config::webUser, Config::webPassword);
  Logger::info("Setup complete");
}

void loop() {
  WiFiManager::loop();
  TimeManager::loop();
  WebServerManager::loop();
  ArduinoOTA.handle();
  if (!BootGuard::safeMode()) {
    MQTTHandler::loop();
    MBusReader::loop();
  }

  BootGuard::loop(WiFiManager::connected(), TimeManager::synchronized(), MBusReader::healthy());
  updateLed();
  delay(2);
}
