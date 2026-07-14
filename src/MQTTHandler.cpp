#include "MQTTHandler.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include "Config.h"
#include "Logger.h"
#include "MBusReader.h"
#include "constants.h"

WiFiClient MQTTHandler::plainClient_;
WiFiClientSecure MQTTHandler::secureClient_;
PubSubClient MQTTHandler::client_(MQTTHandler::plainClient_);
uint32_t MQTTHandler::nextReconnectAt_ = 0;
uint32_t MQTTHandler::reconnectDelayMs_ = 1000;
uint32_t MQTTHandler::reconnectCount_ = 0;
uint32_t MQTTHandler::publishErrors_ = 0;
bool MQTTHandler::discoverySent_ = false;
bool MQTTHandler::restartRequested_ = false;

String MQTTHandler::topic(const char* suffix) {
  String value = Config::mqttBaseTopic;
  if (suffix && suffix[0]) {
    value += '/';
    value += suffix;
  }
  return value;
}

void MQTTHandler::configureTransport() {
  if (!Config::mqttTls) {
    client_.setClient(plainClient_);
    return;
  }
  if (Config::mqttTlsInsecure) {
    secureClient_.setInsecure();
    Logger::warn("MQTT TLS certificate validation disabled");
  } else if (!Config::mqttCaCert.isEmpty()) {
    secureClient_.setCACert(Config::mqttCaCert.c_str());
  } else {
    Logger::error("MQTT TLS enabled without CA certificate");
  }
  client_.setClient(secureClient_);
}

void MQTTHandler::begin() {
  configureTransport();
  client_.setServer(Config::mqttHost, Config::mqttPort);
  client_.setBufferSize(Constants::MQTT_BUFFER_SIZE);
  client_.setKeepAlive(30);
  client_.setSocketTimeout(10);
  client_.setCallback(callback);
}

bool MQTTHandler::connect() {
  if (Config::mqttHost[0] == '\0' || WiFi.status() != WL_CONNECTED) return false;
  if (Config::mqttTls && !Config::mqttTlsInsecure && Config::mqttCaCert.isEmpty()) return false;
  const String availability = topic("availability");
  bool ok = false;
  if (Config::mqttUser[0]) {
    ok = client_.connect(Config::deviceId, Config::mqttUser, Config::mqttPassword,
                         availability.c_str(), 1, true, "offline");
  } else {
    ok = client_.connect(Config::deviceId, availability.c_str(), 1, true, "offline");
  }
  if (!ok) {
    reconnectCount_++;
    Logger::warn("MQTT connect failed, state=" + String(client_.state()));
    return false;
  }
  reconnectDelayMs_ = 1000;
  client_.publish(availability.c_str(), "online", true);
  if (Config::mqttCommandsEnabled) {
    client_.subscribe(topic("command/#").c_str(), 1);
  }
  discoverySent_ = false;
  Logger::info("MQTT connected");
  return true;
}

void MQTTHandler::loop() {
  if (restartRequested_) {
    client_.disconnect();
    delay(100);
    ESP.restart();
  }
  if (!client_.connected()) {
    const uint32_t now = millis();
    if (static_cast<int32_t>(now - nextReconnectAt_) >= 0) {
      if (!connect()) {
        const uint32_t jitter = esp_random() % 1000;
        nextReconnectAt_ = now + reconnectDelayMs_ + jitter;
        const uint32_t doubled = reconnectDelayMs_ * 2UL;
        reconnectDelayMs_ = doubled < Constants::MQTT_RECONNECT_MAX_MS ? doubled : Constants::MQTT_RECONNECT_MAX_MS;
      }
    }
    return;
  }
  client_.loop();
  if (!discoverySent_) sendDiscovery();
}

bool MQTTHandler::connected() { return client_.connected(); }
uint32_t MQTTHandler::reconnectCount() { return reconnectCount_; }
uint32_t MQTTHandler::publishErrors() { return publishErrors_; }

void MQTTHandler::publishReading(const UsageSnapshot& usage) {
  if (!client_.connected()) return;
  JsonDocument doc;
  doc["volume_m3"] = usage.correctedVolumeM3;
  doc["raw_volume_m3"] = usage.rawVolumeM3;
  doc["energy_kwh"] = usage.energyKwh;
  doc["flow_m3h"] = usage.flowM3h;
  doc["day_m3"] = usage.dayM3;
  doc["month_m3"] = usage.monthM3;
  doc["year_m3"] = usage.yearM3;
  doc["day_cost"] = usage.dayCost;
  doc["month_cost"] = usage.monthCost;
  doc["year_cost"] = usage.yearCost;
  doc["calorific_value"] = Config::calorificValue;
  doc["correction_factor"] = Config::correctionFactor;
  doc["timestamp"] = static_cast<uint64_t>(usage.timestamp);
  String payload;
  serializeJson(doc, payload);
  if (!client_.publish(topic("state").c_str(), payload.c_str(), true)) publishErrors_++;
  publishDiagnostics();
}

void MQTTHandler::publishDiagnostics() {
  if (!client_.connected()) return;
  JsonDocument doc;
  const MBusStats& stats = MBusReader::stats();
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["free_heap"] = ESP.getFreeHeap();
  doc["min_free_heap"] = ESP.getMinFreeHeap();
  doc["uptime_seconds"] = millis() / 1000UL;
  doc["mbus_polls"] = stats.polls;
  doc["mbus_success_rate"] = stats.polls ? (stats.successful * 100.0f / stats.polls) : 0.0f;
  doc["mbus_timeouts"] = stats.timeouts;
  doc["mbus_parse_errors"] = stats.parseErrors;
  doc["mqtt_reconnects"] = reconnectCount_;
  String payload;
  serializeJson(doc, payload);
  if (!client_.publish(topic("diagnostics").c_str(), payload.c_str(), true)) publishErrors_++;
}

void MQTTHandler::sendSensorDiscovery(const char* objectId, const char* name, const char* stateTopic,
                                      const char* unit, const char* deviceClass, const char* stateClass,
                                      bool diagnostic) {
  JsonDocument doc;
  String unique = String(Config::deviceId) + '_' + objectId;
  doc["name"] = name;
  doc["unique_id"] = unique;
  doc["object_id"] = unique;
  doc["state_topic"] = stateTopic;
  doc["value_template"] = String("{{ value_json.") + objectId + " }}";
  if (unit && unit[0]) doc["unit_of_measurement"] = unit;
  if (deviceClass && deviceClass[0]) doc["device_class"] = deviceClass;
  if (stateClass && stateClass[0]) doc["state_class"] = stateClass;
  if (diagnostic) doc["entity_category"] = "diagnostic";
  doc["availability_topic"] = topic("availability");
  doc["payload_available"] = "online";
  doc["payload_not_available"] = "offline";
  JsonObject device = doc["device"].to<JsonObject>();
  device["identifiers"].to<JsonArray>().add(Config::deviceId);
  device["name"] = Config::hostname;
  device["manufacturer"] = "ESP32 / community";
  device["model"] = "BK-G4 M-Bus gateway";
  device["sw_version"] = FIRMWARE_VERSION;
  device["configuration_url"] = String("http://") + WiFi.localIP().toString();
  String payload;
  serializeJson(doc, payload);
  const String discoveryTopic = String("homeassistant/sensor/") + unique + "/config";
  if (!client_.publish(discoveryTopic.c_str(), payload.c_str(), true)) publishErrors_++;
}

void MQTTHandler::sendBinaryDiscovery(const char* objectId, const char* name, const char* stateTopic) {
  JsonDocument doc;
  String unique = String(Config::deviceId) + '_' + objectId;
  doc["name"] = name;
  doc["unique_id"] = unique;
  doc["object_id"] = unique;
  doc["state_topic"] = stateTopic;
  doc["payload_on"] = "online";
  doc["payload_off"] = "offline";
  doc["device_class"] = "connectivity";
  JsonObject device = doc["device"].to<JsonObject>();
  device["identifiers"].to<JsonArray>().add(Config::deviceId);
  device["name"] = Config::hostname;
  device["manufacturer"] = "ESP32 / community";
  device["model"] = "BK-G4 M-Bus gateway";
  device["sw_version"] = FIRMWARE_VERSION;
  String payload;
  serializeJson(doc, payload);
  const String discoveryTopic = String("homeassistant/binary_sensor/") + unique + "/config";
  if (!client_.publish(discoveryTopic.c_str(), payload.c_str(), true)) publishErrors_++;
}

void MQTTHandler::sendNumberDiscovery(const char* objectId, const char* name, const char* stateTopic,
                                      const char* commandTopic, float minValue, float maxValue, float step,
                                      const char* unit) {
  JsonDocument doc;
  String unique = String(Config::deviceId) + '_' + objectId;
  doc["name"] = name;
  doc["unique_id"] = unique;
  doc["object_id"] = unique;
  doc["state_topic"] = stateTopic;
  doc["value_template"] = String("{{ value_json.") + objectId + " }}";
  doc["command_topic"] = commandTopic;
  doc["min"] = minValue;
  doc["max"] = maxValue;
  doc["step"] = step;
  doc["mode"] = "box";
  if (unit && unit[0]) doc["unit_of_measurement"] = unit;
  JsonObject device = doc["device"].to<JsonObject>();
  device["identifiers"].to<JsonArray>().add(Config::deviceId);
  device["name"] = Config::hostname;
  String payload;
  serializeJson(doc, payload);
  const String discoveryTopic = String("homeassistant/number/") + unique + "/config";
  if (!client_.publish(discoveryTopic.c_str(), payload.c_str(), true)) publishErrors_++;
}

void MQTTHandler::sendDiscovery() {
  const String state = topic("state");
  const String diagnostics = topic("diagnostics");
  sendSensorDiscovery("volume_m3", "Gas meter", state.c_str(), "m³", "gas", "total_increasing");
  sendSensorDiscovery("energy_kwh", "Gas energy", state.c_str(), "kWh", "energy", "total_increasing");
  sendSensorDiscovery("flow_m3h", "Gas flow", state.c_str(), "m³/h", "volume_flow_rate", "measurement");
  sendSensorDiscovery("day_m3", "Gas today", state.c_str(), "m³", "gas", "total_increasing");
  sendSensorDiscovery("month_m3", "Gas this month", state.c_str(), "m³", "gas", "total_increasing");
  sendSensorDiscovery("year_m3", "Gas this year", state.c_str(), "m³", "gas", "total_increasing");
  sendSensorDiscovery("day_cost", "Gas cost today", state.c_str(), "€", "monetary", "total");
  sendSensorDiscovery("month_cost", "Gas cost this month", state.c_str(), "€", "monetary", "total");
  sendSensorDiscovery("wifi_rssi", "Wi-Fi signal", diagnostics.c_str(), "dBm", "signal_strength", "measurement", true);
  sendSensorDiscovery("free_heap", "Free heap", diagnostics.c_str(), "B", "data_size", "measurement", true);
  sendSensorDiscovery("mbus_success_rate", "M-Bus success rate", diagnostics.c_str(), "%", "", "measurement", true);
  sendBinaryDiscovery("online", "Online", topic("availability").c_str());
  if (Config::mqttCommandsEnabled) {
    sendNumberDiscovery("calorific_value", "Calorific value", state.c_str(), topic("command/calorific_value").c_str(), 5.0f, 20.0f, 0.001f, "kWh/m³");
    sendNumberDiscovery("correction_factor", "Correction factor", state.c_str(), topic("command/correction_factor").c_str(), 0.5f, 1.5f, 0.0001f, "");
  }
  discoverySent_ = true;
  Logger::info("Home Assistant discovery published");
}

void MQTTHandler::callback(char* topicValue, uint8_t* payload, unsigned int length) {
  if (!Config::mqttCommandsEnabled || length > 64) return;
  String commandTopic(topicValue);
  String value;
  value.reserve(length);
  for (unsigned int i = 0; i < length; ++i) value += static_cast<char>(payload[i]);
  value.trim();
  if (commandTopic.endsWith("/poll")) {
    MBusReader::trigger();
  } else if (commandTopic.endsWith("/restart")) {
    restartRequested_ = true;
  } else if (commandTopic.endsWith("/calorific_value")) {
    const float parsed = value.toFloat();
    if (parsed >= 5.0f && parsed <= 20.0f) {
      Config::calorificValue = parsed;
      Config::save();
    }
  } else if (commandTopic.endsWith("/correction_factor")) {
    const float parsed = value.toFloat();
    if (parsed >= 0.5f && parsed <= 1.5f) {
      Config::correctionFactor = parsed;
      Config::save();
    }
  }
}
