#include "MQTTHandler.h"
#include <cstdlib>
#include <cstring>
#include <WiFi.h>
#include "BootGuard.h"
#include "Config.h"
#include "Logger.h"
#include "MBusReader.h"
#include "TimeManager.h"
#include "constants.h"

WiFiClient MQTTHandler::plainClient_;
WiFiClientSecure MQTTHandler::secureClient_;
PubSubClient MQTTHandler::client_(MQTTHandler::plainClient_);
uint32_t MQTTHandler::nextReconnectAt_ = 0;
uint32_t MQTTHandler::reconnectDelayMs_ = 1000;
uint32_t MQTTHandler::reconnectCount_ = 0;
uint32_t MQTTHandler::publishErrors_ = 0;
uint32_t MQTTHandler::lastDiagnosticsPublishAt_ = 0;
bool MQTTHandler::discoverySent_ = false;
bool MQTTHandler::restartRequested_ = false;

namespace {
bool parseFloatValue(const String& value, float& result) {
  char* end = nullptr;
  result = std::strtof(value.c_str(), &end);
  return end && end != value.c_str() && *end == '\0';
}
}

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
  if (Config::mqttCommandsEnabled) client_.subscribe(topic("command/#").c_str(), 1);
  discoverySent_ = false;
  Logger::info("MQTT connected at " + String(Config::mqttBaseTopic));
  return true;
}

void MQTTHandler::loop() {
  if (restartRequested_) {
    BootGuard::markPlannedRestart("mqtt_restart");
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
  if (millis() - lastDiagnosticsPublishAt_ >= 60000UL) publishDiagnostics();
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
  doc["day_energy_kwh"] = usage.dayEnergyKwh;
  doc["month_energy_kwh"] = usage.monthEnergyKwh;
  doc["year_energy_kwh"] = usage.yearEnergyKwh;
  doc["calorific_value"] = Config::calorificValue;
  doc["correction_factor"] = Config::correctionFactor;
  doc["poll_interval_seconds"] = Config::pollIntervalMs / 1000UL;
  doc["max_flow_m3h"] = Config::maxFlowM3h;
  doc["continuous_flow_threshold_m3h"] = Config::continuousFlowThresholdM3h;
  doc["continuous_flow_alert_minutes"] = Config::continuousFlowAlertMinutes;
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
  const UsageSnapshot& usage = UsageTracker::snapshot();
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["free_heap"] = ESP.getFreeHeap();
  doc["min_free_heap"] = ESP.getMinFreeHeap();
  doc["uptime_seconds"] = millis() / 1000UL;
  doc["mbus_polls"] = stats.polls;
  doc["mbus_success_rate"] = stats.polls ? (stats.successful * 100.0f / stats.polls) : 0.0f;
  doc["mbus_timeouts"] = stats.timeouts;
  doc["mbus_parse_errors"] = stats.parseErrors;
  doc["mqtt_reconnects"] = reconnectCount_;
  doc["mqtt_publish_errors"] = publishErrors_;
  doc["time_synchronized"] = TimeManager::synchronized();
  doc["mbus_healthy"] = MBusReader::healthy();
  doc["continuous_flow_alert"] = usage.continuousFlowAlert;
  doc["continuous_flow_minutes"] = usage.continuousFlowMinutes;
  doc["boot_failures"] = BootGuard::failureCount();
  doc["ota_pending_validation"] = BootGuard::pendingOtaValidation();
  String payload;
  serializeJson(doc, payload);
  if (!client_.publish(topic("diagnostics").c_str(), payload.c_str(), true)) publishErrors_++;
  lastDiagnosticsPublishAt_ = millis();
}

void MQTTHandler::addDevice(JsonObject device) {
  device["identifiers"].to<JsonArray>().add(Config::deviceId);
  device["name"] = Config::hostname;
  device["manufacturer"] = "ESP32 / community";
  device["model"] = "BK-G4 M-Bus gateway";
  device["sw_version"] = FIRMWARE_VERSION;
  device["configuration_url"] = String("http://") + WiFi.localIP().toString();
}

void MQTTHandler::sendSensorDiscovery(const char* objectId, const char* name, const char* stateTopic,
                                      const char* unit, const char* deviceClass, const char* stateClass,
                                      bool diagnostic) {
  JsonDocument doc;
  const String unique = String(Config::deviceId) + '_' + objectId;
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
  addDevice(doc["device"].to<JsonObject>());
  String payload;
  serializeJson(doc, payload);
  const String discoveryTopic = String("homeassistant/sensor/") + unique + "/config";
  if (!client_.publish(discoveryTopic.c_str(), payload.c_str(), true)) publishErrors_++;
}

void MQTTHandler::sendBinaryDiscovery(const char* objectId, const char* name, const char* stateTopic,
                                      const char* valueTemplate, const char* payloadOn, const char* payloadOff,
                                      bool diagnostic) {
  JsonDocument doc;
  const String unique = String(Config::deviceId) + '_' + objectId;
  doc["name"] = name;
  doc["unique_id"] = unique;
  doc["object_id"] = unique;
  doc["state_topic"] = stateTopic;
  if (valueTemplate && valueTemplate[0]) doc["value_template"] = valueTemplate;
  doc["payload_on"] = payloadOn;
  doc["payload_off"] = payloadOff;
  if (strcmp(objectId, "online") == 0) doc["device_class"] = "connectivity";
  if (strcmp(objectId, "continuous_flow_alert") == 0) doc["device_class"] = "problem";
  if (diagnostic) doc["entity_category"] = "diagnostic";
  if (strcmp(objectId, "online") != 0) doc["availability_topic"] = topic("availability");
  addDevice(doc["device"].to<JsonObject>());
  String payload;
  serializeJson(doc, payload);
  const String discoveryTopic = String("homeassistant/binary_sensor/") + unique + "/config";
  if (!client_.publish(discoveryTopic.c_str(), payload.c_str(), true)) publishErrors_++;
}

void MQTTHandler::sendNumberDiscovery(const char* objectId, const char* name, const char* stateTopic,
                                      const char* commandTopic, float minValue, float maxValue, float step,
                                      const char* unit) {
  JsonDocument doc;
  const String unique = String(Config::deviceId) + '_' + objectId;
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
  doc["entity_category"] = "config";
  if (unit && unit[0]) doc["unit_of_measurement"] = unit;
  doc["availability_topic"] = topic("availability");
  addDevice(doc["device"].to<JsonObject>());
  String payload;
  serializeJson(doc, payload);
  const String discoveryTopic = String("homeassistant/number/") + unique + "/config";
  if (!client_.publish(discoveryTopic.c_str(), payload.c_str(), true)) publishErrors_++;
}

void MQTTHandler::sendButtonDiscovery(const char* objectId, const char* name, const char* commandTopic,
                                      const char* payloadValue, const char* deviceClass) {
  JsonDocument doc;
  const String unique = String(Config::deviceId) + '_' + objectId;
  doc["name"] = name;
  doc["unique_id"] = unique;
  doc["object_id"] = unique;
  doc["command_topic"] = commandTopic;
  doc["payload_press"] = payloadValue;
  doc["entity_category"] = "config";
  if (deviceClass && deviceClass[0]) doc["device_class"] = deviceClass;
  doc["availability_topic"] = topic("availability");
  addDevice(doc["device"].to<JsonObject>());
  String payload;
  serializeJson(doc, payload);
  const String discoveryTopic = String("homeassistant/button/") + unique + "/config";
  if (!client_.publish(discoveryTopic.c_str(), payload.c_str(), true)) publishErrors_++;
}

void MQTTHandler::cleanupDiscovery(const char* component, const char* objectId) {
  const String unique = String(Config::deviceId) + '_' + objectId;
  const String discoveryTopic = String("homeassistant/") + component + '/' + unique + "/config";
  if (!client_.publish(discoveryTopic.c_str(), "", true)) publishErrors_++;
}

void MQTTHandler::sendDiscovery() {
  const String state = topic("state");
  const String diagnostics = topic("diagnostics");
  const String availability = topic("availability");

  sendSensorDiscovery("volume_m3", "Gas meter", state.c_str(), "m³", "gas", "total_increasing");
  sendSensorDiscovery("energy_kwh", "Gas energy", state.c_str(), "kWh", "energy", "total_increasing");
  sendSensorDiscovery("flow_m3h", "Gas flow", state.c_str(), "m³/h", "volume_flow_rate", "measurement");
  sendSensorDiscovery("day_m3", "Gas today", state.c_str(), "m³", "gas", "total_increasing");
  sendSensorDiscovery("month_m3", "Gas this month", state.c_str(), "m³", "gas", "total_increasing");
  sendSensorDiscovery("year_m3", "Gas this year", state.c_str(), "m³", "gas", "total_increasing");
  sendSensorDiscovery("day_energy_kwh", "Gas energy today", state.c_str(), "kWh", "energy", "total_increasing");
  sendSensorDiscovery("month_energy_kwh", "Gas energy this month", state.c_str(), "kWh", "energy", "total_increasing");
  sendSensorDiscovery("year_energy_kwh", "Gas energy this year", state.c_str(), "kWh", "energy", "total_increasing");

  sendSensorDiscovery("wifi_rssi", "Wi-Fi signal", diagnostics.c_str(), "dBm", "signal_strength", "measurement", true);
  sendSensorDiscovery("free_heap", "Free heap", diagnostics.c_str(), "B", "data_size", "measurement", true);
  sendSensorDiscovery("uptime_seconds", "Uptime", diagnostics.c_str(), "s", "duration", "total_increasing", true);
  sendSensorDiscovery("mbus_success_rate", "M-Bus success rate", diagnostics.c_str(), "%", "", "measurement", true);
  sendSensorDiscovery("mbus_timeouts", "M-Bus timeouts", diagnostics.c_str(), "", "", "total_increasing", true);
  sendSensorDiscovery("mbus_parse_errors", "M-Bus parse errors", diagnostics.c_str(), "", "", "total_increasing", true);
  sendSensorDiscovery("mqtt_reconnects", "MQTT reconnects", diagnostics.c_str(), "", "", "total_increasing", true);
  sendSensorDiscovery("mqtt_publish_errors", "MQTT publish errors", diagnostics.c_str(), "", "", "total_increasing", true);
  sendSensorDiscovery("continuous_flow_minutes", "Continuous gas flow", diagnostics.c_str(), "min", "duration", "measurement", true);
  sendSensorDiscovery("boot_failures", "Unstable boot count", diagnostics.c_str(), "", "", "measurement", true);

  sendBinaryDiscovery("online", "Online", availability.c_str(), "", "online", "offline", true);
  sendBinaryDiscovery("time_synchronized", "Time synchronized", diagnostics.c_str(),
                      "{{ 'ON' if value_json.time_synchronized else 'OFF' }}", "ON", "OFF", true);
  sendBinaryDiscovery("mbus_healthy", "M-Bus healthy", diagnostics.c_str(),
                      "{{ 'ON' if value_json.mbus_healthy else 'OFF' }}", "ON", "OFF", true);
  sendBinaryDiscovery("continuous_flow_alert", "Continuous gas flow warning", diagnostics.c_str(),
                      "{{ 'ON' if value_json.continuous_flow_alert else 'OFF' }}", "ON", "OFF", true);
  sendBinaryDiscovery("ota_pending_validation", "OTA pending validation", diagnostics.c_str(),
                      "{{ 'ON' if value_json.ota_pending_validation else 'OFF' }}", "ON", "OFF", true);

  cleanupDiscovery("sensor", "day_cost");
  cleanupDiscovery("sensor", "month_cost");
  cleanupDiscovery("sensor", "year_cost");

  if (Config::mqttCommandsEnabled) {
    sendNumberDiscovery("calorific_value", "Calorific value", state.c_str(), topic("command/calorific_value").c_str(), 5.0f, 20.0f, 0.001f, "kWh/m³");
    sendNumberDiscovery("correction_factor", "Correction factor", state.c_str(), topic("command/correction_factor").c_str(), 0.5f, 1.5f, 0.0001f, "");
    sendNumberDiscovery("poll_interval_seconds", "Poll interval", state.c_str(), topic("command/poll_interval_seconds").c_str(), 10.0f, 3600.0f, 1.0f, "s");
    sendNumberDiscovery("max_flow_m3h", "Maximum plausible flow", state.c_str(), topic("command/max_flow_m3h").c_str(), 0.1f, 100.0f, 0.1f, "m³/h");
    sendNumberDiscovery("continuous_flow_threshold_m3h", "Continuous flow threshold", state.c_str(), topic("command/continuous_flow_threshold_m3h").c_str(), 0.0f, 10.0f, 0.001f, "m³/h");
    sendNumberDiscovery("continuous_flow_alert_minutes", "Continuous flow warning delay", state.c_str(), topic("command/continuous_flow_alert_minutes").c_str(), 0.0f, 10080.0f, 1.0f, "min");
    sendButtonDiscovery("poll", "Poll M-Bus now", topic("command/poll").c_str(), "PRESS");
    sendButtonDiscovery("restart", "Restart", topic("command/restart").c_str(), "PRESS", "restart");
  } else {
    const char* numbers[] = {"calorific_value", "correction_factor", "poll_interval_seconds", "max_flow_m3h", "continuous_flow_threshold_m3h", "continuous_flow_alert_minutes"};
    for (const char* objectId : numbers) cleanupDiscovery("number", objectId);
    cleanupDiscovery("button", "poll");
    cleanupDiscovery("button", "restart");
  }

  discoverySent_ = true;
  Logger::info("Home Assistant discovery published");
}

void MQTTHandler::callback(char* topicValue, uint8_t* payload, unsigned int length) {
  if (!Config::mqttCommandsEnabled || length > 64) return;
  const String commandTopic(topicValue);
  String value;
  value.reserve(length);
  for (unsigned int i = 0; i < length; ++i) value += static_cast<char>(payload[i]);
  value.trim();

  if (commandTopic.endsWith("/poll")) {
    MBusReader::trigger();
    return;
  }
  if (commandTopic.endsWith("/restart")) {
    restartRequested_ = true;
    return;
  }

  float parsed = 0.0f;
  if (!parseFloatValue(value, parsed)) return;
  bool changed = false;
  if (commandTopic.endsWith("/calorific_value") && parsed >= 5.0f && parsed <= 20.0f) {
    Config::calorificValue = parsed;
    changed = true;
  } else if (commandTopic.endsWith("/correction_factor") && parsed >= 0.5f && parsed <= 1.5f) {
    Config::correctionFactor = parsed;
    changed = true;
  } else if (commandTopic.endsWith("/poll_interval_seconds") && parsed >= 10.0f && parsed <= 3600.0f) {
    Config::pollIntervalMs = static_cast<uint32_t>(parsed * 1000.0f);
    changed = true;
  } else if (commandTopic.endsWith("/max_flow_m3h") && parsed > 0.0f && parsed <= 100.0f) {
    Config::maxFlowM3h = parsed;
    changed = true;
  } else if (commandTopic.endsWith("/continuous_flow_threshold_m3h") && parsed >= 0.0f && parsed <= Config::maxFlowM3h) {
    Config::continuousFlowThresholdM3h = parsed;
    changed = true;
  } else if (commandTopic.endsWith("/continuous_flow_alert_minutes") && parsed >= 0.0f && parsed <= 10080.0f) {
    Config::continuousFlowAlertMinutes = static_cast<uint32_t>(parsed);
    changed = true;
  }

  if (changed && Config::save()) publishReading(UsageTracker::snapshot());
}
