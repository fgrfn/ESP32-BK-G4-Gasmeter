#include "MQTTHandler.h"
#include "Config.h"
#include "Logger.h"

// Static member initialization
PubSubClient* MQTTHandler::client = nullptr;
bool MQTTHandler::haDiscoverySent = false;
ErrorStats MQTTHandler::errorStats;

void MQTTHandler::init(WiFiClient& wifiClient) {
  if (client == nullptr) {
    client = new PubSubClient(wifiClient);
  }
  client->setServer(Config::mqtt_server, Config::mqtt_port);
  client->setBufferSize(MQTT_BUFFER_SIZE);
  haDiscoverySent = false;
  resetErrorStats();
}

void MQTTHandler::resetErrorStats() {
  errorStats.mbusTimeouts = 0;
  errorStats.mbusParseErrors = 0;
  errorStats.mqttErrors = 0;
  errorStats.wifiDisconnects = 0;
  errorStats.lastError = 0;
  memset(errorStats.lastErrorMsg, 0, sizeof(errorStats.lastErrorMsg));
}

void MQTTHandler::logError(const char* msg) {
  errorStats.lastError = millis();
  strncpy(errorStats.lastErrorMsg, msg, sizeof(errorStats.lastErrorMsg) - 1);
  errorStats.lastErrorMsg[sizeof(errorStats.lastErrorMsg) - 1] = '\0';
  Serial.print("ERROR: ");
  Serial.println(msg);
}

bool MQTTHandler::connect() {
  if (!client) return false;
  if (client->connected()) return true;
  
  Serial.print("Verbinde mit MQTT Broker: ");
  Serial.println(Config::mqtt_server);
  
  bool connected = false;
  if (strlen(Config::mqtt_user) > 0) {
    connected = client->connect(Config::mqtt_client_id, 
                                Config::mqtt_user, 
                                Config::mqtt_pass,
                                Config::mqtt_availability_topic, 
                                0, true, "offline");
  } else {
    connected = client->connect(Config::mqtt_client_id, 
                                Config::mqtt_availability_topic, 
                                0, true, "offline");
  }
  
  if (connected) {
    Serial.println("MQTT verbunden!");
    Logger::addLog("MQTT: Verbunden mit Broker");
    
    // Online-Status publishen
    client->publish(Config::mqtt_availability_topic, "online", true);
    
    // Discovery Flag zurücksetzen für erneutes Senden
    haDiscoverySent = false;
    return true;
  } else {
    Serial.print("MQTT Verbindung fehlgeschlagen, rc=");
    Serial.println(client->state());
    errorStats.mqttErrors++;
    logError("MQTT Verbindung fehlgeschlagen");
    return false;
  }
}

void MQTTHandler::loop() {
  if (client) {
    client->loop();
  }
}

bool MQTTHandler::isConnected() {
  return client && client->connected();
}

bool MQTTHandler::publishVolume(float volume) {
  if (!isConnected()) return false;
  
  char payload[16];
  dtostrf(volume, 0, 2, payload);
  
  if (client->publish(Config::mqtt_topic, payload, true)) {
    Serial.print("Verbrauch gesendet: ");
    Serial.println(payload);
    char msg[60];
    snprintf(msg, sizeof(msg), "M-Bus: Verbrauch OK - %s m3", payload);
    Logger::addLog(msg);
    return true;
  }
  
  errorStats.mqttErrors++;
  logError("MQTT Publish fehlgeschlagen");
  Logger::addLog("MQTT: Publish Fehler");
  return false;
}

bool MQTTHandler::publishEnergy(float energy) {
  if (!isConnected()) return false;
  
  char payload[16];
  dtostrf(energy, 0, 1, payload);
  
  String topic = String(Config::mqtt_topic) + "_energy";
  if (client->publish(topic.c_str(), payload, true)) {
    Serial.print("Energie gesendet: ");
    Serial.print(payload);
    Serial.println(" kWh");
    return true;
  }
  
  return false;
}

bool MQTTHandler::publishFlow(float flow) {
  if (!isConnected()) return false;
  
  String topic = String(Config::mqtt_topic) + "_flow";
  char payload[16];
  dtostrf(flow, 0, 4, payload);
  return client->publish(topic.c_str(), payload, true);
}

bool MQTTHandler::publishBrennwert(float value) {
  if (!isConnected()) return false;
  
  String topic = String(Config::mqtt_topic) + "_brennwert";
  char payload[16];
  dtostrf(value, 0, 6, payload);
  return client->publish(topic.c_str(), payload, true);
}

bool MQTTHandler::publishZZahl(float value) {
  if (!isConnected()) return false;
  
  String topic = String(Config::mqtt_topic) + "_z";
  char payload[16];
  dtostrf(value, 0, 6, payload);
  return client->publish(topic.c_str(), payload, true);
}

bool MQTTHandler::publishWiFiSignal(int rssi) {
  if (!isConnected()) return false;
  
  String topic = String(Config::mqtt_topic) + "_wifi";
  return client->publish(topic.c_str(), String(rssi).c_str(), true);
}

bool MQTTHandler::publishMBusRate(float rate) {
  if (!isConnected()) return false;
  
  String topic = String(Config::mqtt_topic) + "_mbus_rate";
  char payload[16];
  dtostrf(rate, 0, 1, payload);
  return client->publish(topic.c_str(), payload, true);
}

void MQTTHandler::sendHomeAssistantDiscovery() {
  if (!isConnected() || haDiscoverySent) return;
  
  Serial.println("Sende Home Assistant Discovery Messages...");
  Logger::addLog("MQTT: Sende HA Discovery");
  
  // Zählerstand (m³)
  sendDiscoverySensor("Zaehlerstand", "gas", "m³", Config::mqtt_topic, "total_increasing");
  
  // Energie (kWh) für Energy Dashboard
  String energyTopic = String(Config::mqtt_topic) + "_energy";
  sendDiscoverySensor("Gasverbrauch", "energy", "kWh", energyTopic.c_str(), "total_increasing");
  
  // Gasdurchfluss (m³/h)
  String flowTopic = String(Config::mqtt_topic) + "_flow";
  sendDiscoverySensor("Flow", "volume_flow_rate", "m³/h", flowTopic.c_str(), "measurement");
  
  // Brennwert
  String brennwertTopic = String(Config::mqtt_topic) + "_brennwert";
  sendDiscoverySensor("Brennwert", nullptr, "kWh/m³", brennwertTopic.c_str(), "measurement");
  
  // Z-Zahl
  String zTopic = String(Config::mqtt_topic) + "_z";
  sendDiscoverySensor("Z-Zahl", nullptr, "", zTopic.c_str(), "measurement");
  
  // WiFi Signal
  String wifiTopic = String(Config::mqtt_topic) + "_wifi";
  sendDiscoverySensor("WiFi", "signal_strength", "dBm", wifiTopic.c_str(), "measurement");
  
  // M-Bus Success Rate
  String rateTopic = String(Config::mqtt_topic) + "_mbus_rate";
  sendDiscoverySensor("M-Bus", nullptr, "%", rateTopic.c_str(), "measurement");
  
  // Binary Sensor für Availability
  sendDiscoveryBinarySensor("Online", "connectivity", Config::mqtt_availability_topic);
  
  haDiscoverySent = true;
  Serial.println("Home Assistant Discovery abgeschlossen");
  Logger::addLog("MQTT: HA Discovery gesendet");
}

void MQTTHandler::sendDiscoverySensor(const char* name, const char* deviceClass, 
                                       const char* unit, const char* topic,
                                       const char* stateClass) {
  if (!client) return;
  
  String discoveryTopic = "homeassistant/sensor/esp32_gaszaehler_";
  discoveryTopic += name;
  discoveryTopic += "/config";
  
  String payload = "{";
  payload += "\"name\":\"ESP32 Gaszaehler ";
  payload += name;
  payload += "\",";
  payload += "\"unique_id\":\"esp32_gas_";
  payload += name;
  payload += "\",";
  payload += "\"state_topic\":\"";
  payload += topic;
  payload += "\",";
  
  if (deviceClass) {
    payload += "\"device_class\":\"";
    payload += deviceClass;
    payload += "\",";
  }
  
  if (stateClass) {
    payload += "\"state_class\":\"";
    payload += stateClass;
    payload += "\",";
  }
  
  payload += "\"unit_of_measurement\":\"";
  payload += unit;
  payload += "\",";
  payload += "\"availability_topic\":\"";
  payload += Config::mqtt_availability_topic;
  payload += "\",";
  payload += "\"device\":{";
  payload += "\"identifiers\":[\"esp32_gaszaehler\"],";
  payload += "\"name\":\"ESP32 Gaszaehler\",";
  payload += "\"model\":\"ESP32-WROOM-32D\",";
  payload += "\"manufacturer\":\"Espressif\",";
  payload += "\"sw_version\":\"" FIRMWARE_VERSION "\"";
  payload += "}}";
  
  client->publish(discoveryTopic.c_str(), payload.c_str(), true);
  delay(50); // Kleine Pause zwischen Discovery Messages
}

void MQTTHandler::sendDiscoveryBinarySensor(const char* name, const char* deviceClass, 
                                             const char* topic) {
  if (!client) return;
  
  String discoveryTopic = "homeassistant/binary_sensor/esp32_gaszaehler_";
  discoveryTopic += name;
  discoveryTopic += "/config";
  
  String payload = "{";
  payload += "\"name\":\"ESP32 Gaszaehler ";
  payload += name;
  payload += "\",";
  payload += "\"unique_id\":\"esp32_gas_";
  payload += name;
  payload += "\",";
  payload += "\"state_topic\":\"";
  payload += topic;
  payload += "\",";
  payload += "\"payload_on\":\"online\",";
  payload += "\"payload_off\":\"offline\",";
  
  if (deviceClass) {
    payload += "\"device_class\":\"";
    payload += deviceClass;
    payload += "\",";
  }
  
  payload += "\"device\":{";
  payload += "\"identifiers\":[\"esp32_gaszaehler\"],";
  payload += "\"name\":\"ESP32 Gaszaehler\",";
  payload += "\"model\":\"ESP32-WROOM-32D\",";
  payload += "\"manufacturer\":\"Espressif\",";
  payload += "\"sw_version\":\"" FIRMWARE_VERSION "\"";
  payload += "}}";
  
  client->publish(discoveryTopic.c_str(), payload.c_str(), true);
  delay(50);
}
