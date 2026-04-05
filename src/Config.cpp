#include "Config.h"
#include "Logger.h"
#include <WiFi.h>
#include <ArduinoJson.h>

// Static member initialization
char Config::ssid[SSID_MAX_LEN];
char Config::password[PASSWORD_MAX_LEN];
char Config::hostname[HOSTNAME_MAX_LEN];
bool Config::use_static_ip = false;
char Config::static_ip[IP_ADDRESS_MAX_LEN];
char Config::static_gateway[IP_ADDRESS_MAX_LEN];
char Config::static_subnet[IP_ADDRESS_MAX_LEN];
char Config::static_dns[IP_ADDRESS_MAX_LEN];

char Config::mqtt_server[MQTT_SERVER_MAX_LEN];
int Config::mqtt_port = DEFAULT_MQTT_PORT;
char Config::mqtt_user[MQTT_USER_MAX_LEN];
char Config::mqtt_pass[MQTT_PASS_MAX_LEN];
char Config::mqtt_topic[MQTT_TOPIC_MAX_LEN];
char Config::mqtt_availability_topic[MQTT_TOPIC_MAX_LEN];
char Config::mqtt_client_id[MQTT_CLIENT_ID_MAX_LEN];

unsigned long Config::poll_interval = DEFAULT_POLL_INTERVAL;
float Config::gas_calorific_value = DEFAULT_GAS_CALORIFIC;
float Config::gas_correction_factor = DEFAULT_GAS_CORRECTION;
float Config::gas_base_price_monthly = 10.0; // Default: 10€/Monat
float Config::gas_work_price_per_kwh = 0.12; // Default: 0.12€/kWh

bool Config::enable_deep_sleep = false;
unsigned long Config::deep_sleep_duration = 300;

Preferences Config::preferences;

void Config::init() {
  setDefaults();
}

void Config::setDefaults() {
  strcpy(ssid, DEFAULT_SSID);
  strcpy(password, DEFAULT_PASSWORD);
  strcpy(hostname, DEFAULT_HOSTNAME);
  strcpy(mqtt_server, DEFAULT_MQTT_SERVER);
  mqtt_port = DEFAULT_MQTT_PORT;
  strcpy(mqtt_topic, DEFAULT_MQTT_TOPIC);
  strcpy(mqtt_user, "");
  strcpy(mqtt_pass, "");
  poll_interval = DEFAULT_POLL_INTERVAL;
  gas_calorific_value = DEFAULT_GAS_CALORIFIC;
  gas_correction_factor = DEFAULT_GAS_CORRECTION;
  gas_base_price_monthly = 10.0;
  gas_work_price_per_kwh = 0.12;
  use_static_ip = false;
  strcpy(static_ip, DEFAULT_STATIC_IP);
  strcpy(static_gateway, DEFAULT_GATEWAY);
  strcpy(static_subnet, DEFAULT_SUBNET);
  strcpy(static_dns, DEFAULT_DNS);
  
  // Availability Topic generieren
  snprintf(mqtt_availability_topic, sizeof(mqtt_availability_topic), 
           "%s_availability", mqtt_topic);
}

void Config::load() {
  if (!preferences.begin("gas-config", false)) {
    Serial.println("ERROR: Konnte gas-config Namespace nicht oeffnen!");
    return;
  }
  
  bool configDone = preferences.getBool("config_done", false);
  
  preferences.getString("ssid", ssid, sizeof(ssid));
  preferences.getString("password", password, sizeof(password));
  preferences.getString("hostname", hostname, sizeof(hostname));
  preferences.getString("mqtt_server", mqtt_server, sizeof(mqtt_server));
  mqtt_port = preferences.getInt("mqtt_port", DEFAULT_MQTT_PORT);
  preferences.getString("mqtt_user", mqtt_user, sizeof(mqtt_user));
  preferences.getString("mqtt_pass", mqtt_pass, sizeof(mqtt_pass));
  preferences.getString("mqtt_topic", mqtt_topic, sizeof(mqtt_topic));
  poll_interval = preferences.getULong("poll_interval", DEFAULT_POLL_INTERVAL);
  DEBUG_LOG("loadConfig: poll_interval aus Flash = " + String(poll_interval) + " ms");
  gas_calorific_value = preferences.getFloat("gas_calorific", DEFAULT_GAS_CALORIFIC);
  gas_correction_factor = preferences.getFloat("gas_correction", DEFAULT_GAS_CORRECTION);
  gas_base_price_monthly = preferences.getFloat("gas_base_price", 10.0);
  gas_work_price_per_kwh = preferences.getFloat("gas_work_price", 0.12);
  use_static_ip = preferences.getBool("use_static_ip", false);
  preferences.getString("static_ip", static_ip, sizeof(static_ip));
  preferences.getString("static_gateway", static_gateway, sizeof(static_gateway));
  preferences.getString("static_subnet", static_subnet, sizeof(static_subnet));
  preferences.getString("static_dns", static_dns, sizeof(static_dns));
  preferences.end();
  
  // Validierung
  validatePollInterval();
  
  // Wenn noch nie konfiguriert oder SSID leer -> Defaults setzen
  if (!configDone || strlen(ssid) == 0) {
    Serial.println("Keine gueltige Konfiguration gefunden - verwende Defaults");
    strcpy(ssid, DEFAULT_SSID);
    strcpy(password, DEFAULT_PASSWORD);
  }
  
  // Fallback auf Defaults wenn leer
  if (strlen(hostname) == 0) strcpy(hostname, DEFAULT_HOSTNAME);
  if (strlen(mqtt_server) == 0) strcpy(mqtt_server, DEFAULT_MQTT_SERVER);
  if (strlen(mqtt_topic) == 0) strcpy(mqtt_topic, DEFAULT_MQTT_TOPIC);
  
  // Availability Topic generieren
  snprintf(mqtt_availability_topic, sizeof(mqtt_availability_topic), 
           "%s_availability", mqtt_topic);
}

void Config::save() {
  // Validierung vor dem Speichern
  validatePollInterval();
  
  preferences.begin("gas-config", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.putString("hostname", hostname);
  preferences.putString("mqtt_server", mqtt_server);
  preferences.putInt("mqtt_port", mqtt_port);
  preferences.putString("mqtt_user", mqtt_user);
  preferences.putString("mqtt_pass", mqtt_pass);
  preferences.putString("mqtt_topic", mqtt_topic);
  preferences.putULong("poll_interval", poll_interval);
  unsigned long rb_after = preferences.getULong("poll_interval", 0);
  preferences.putFloat("gas_calorific", gas_calorific_value);
  preferences.putFloat("gas_correction", gas_correction_factor);
  preferences.putFloat("gas_base_price", gas_base_price_monthly);
  preferences.putFloat("gas_work_price", gas_work_price_per_kwh);
  preferences.putBool("use_static_ip", use_static_ip);
  preferences.putString("static_ip", static_ip);
  preferences.putString("static_gateway", static_gateway);
  preferences.putString("static_subnet", static_subnet);
  preferences.putString("static_dns", static_dns);
  preferences.putBool("config_done", true);
  preferences.end();

  Serial.println("Konfiguration gespeichert");
  char msg[100];
  snprintf(msg, sizeof(msg), "Poll-Intervall: %lus (%lums) saved_readback=%lu ms", 
           poll_interval / 1000, poll_interval, rb_after);
  Serial.println(msg);
}

bool Config::validatePollInterval() {
  DEBUG_LOG("validatePollInterval: poll_interval vor Validierung = " + String(poll_interval) + " ms");
  
  if (poll_interval < MIN_POLL_INTERVAL) {
    Serial.println("WARN: Ungueltiger poll_interval: " + String(poll_interval) + 
                   " ms - setze auf Default " + String(DEFAULT_POLL_INTERVAL) + " ms");
    poll_interval = DEFAULT_POLL_INTERVAL;
    return false;
  }
  
  if (poll_interval > MAX_POLL_INTERVAL) {
    poll_interval = MAX_POLL_INTERVAL;
    return false;
  }
  
  DEBUG_LOG("validatePollInterval: poll_interval nach Validierung = " + String(poll_interval) + " ms");
  return true;
}

void Config::generateMqttClientId() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(mqtt_client_id, sizeof(mqtt_client_id), 
           "ESP32Gas-%02X%02X%02X", mac[3], mac[4], mac[5]);
  Serial.println("MQTT Client-ID: " + String(mqtt_client_id));
}

// ==========================================
// BACKUP & RESTORE FUNCTIONS
// ==========================================

String Config::exportToJson(bool includePasswords) {
  JsonDocument doc;
  
  // Metadata
  doc["version"] = "2.1.0";
  doc["timestamp"] = millis() / 1000; // Unix timestamp (approximation)
  doc["device"] = mqtt_client_id;
  
  // WiFi Configuration
  JsonObject wifi = doc["wifi"].to<JsonObject>();
  wifi["ssid"] = ssid;
  wifi["password"] = includePasswords ? password : "***NOT_EXPORTED***";
  wifi["hostname"] = hostname;
  wifi["use_static_ip"] = use_static_ip;
  wifi["static_ip"] = static_ip;
  wifi["static_gateway"] = static_gateway;
  wifi["static_subnet"] = static_subnet;
  wifi["static_dns"] = static_dns;
  
  // MQTT Configuration
  JsonObject mqtt = doc["mqtt"].to<JsonObject>();
  mqtt["server"] = mqtt_server;
  mqtt["port"] = mqtt_port;
  mqtt["user"] = mqtt_user;
  mqtt["password"] = includePasswords ? mqtt_pass : "***NOT_EXPORTED***";
  mqtt["topic"] = mqtt_topic;
  
  // Gas Configuration
  JsonObject gas = doc["gas"].to<JsonObject>();
  gas["calorific_value"] = gas_calorific_value;
  gas["correction_factor"] = gas_correction_factor;
  gas["poll_interval_seconds"] = poll_interval / 1000;
  gas["base_price_monthly"] = gas_base_price_monthly;
  gas["work_price_per_kwh"] = gas_work_price_per_kwh;
  
  // Deep Sleep (optional)
  JsonObject sleep = doc["deep_sleep"].to<JsonObject>();
  sleep["enabled"] = enable_deep_sleep;
  sleep["duration_seconds"] = deep_sleep_duration;
  
  String output;
  serializeJsonPretty(doc, output);
  return output;
}

bool Config::importFromJson(const String& json, String* errorMsg) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, json);
  
  if (error) {
    if (errorMsg) *errorMsg = "JSON Parse Error: " + String(error.c_str());
    return false;
  }
  
  // Validate first
  if (!validateJson(json, errorMsg)) {
    return false;
  }
  
  // Import WiFi Configuration
  if (doc["wifi"]["ssid"].is<const char*>()) {
    strlcpy(ssid, doc["wifi"]["ssid"], sizeof(ssid));
  }
  
  if (doc["wifi"]["password"].is<const char*>()) {
    const char* pwd = doc["wifi"]["password"];
    // Skip if placeholder
    if (strcmp(pwd, "***NOT_EXPORTED***") != 0) {
      strlcpy(password, pwd, sizeof(password));
    }
  }
  
  if (doc["wifi"]["hostname"].is<const char*>()) {
    strlcpy(hostname, doc["wifi"]["hostname"], sizeof(hostname));
  }
  
  if (doc["wifi"]["use_static_ip"].is<bool>()) {
    use_static_ip = doc["wifi"]["use_static_ip"];
  }
  
  if (doc["wifi"]["static_ip"].is<const char*>()) {
    strlcpy(static_ip, doc["wifi"]["static_ip"], sizeof(static_ip));
  }
  
  if (doc["wifi"]["static_gateway"].is<const char*>()) {
    strlcpy(static_gateway, doc["wifi"]["static_gateway"], sizeof(static_gateway));
  }
  
  if (doc["wifi"]["static_subnet"].is<const char*>()) {
    strlcpy(static_subnet, doc["wifi"]["static_subnet"], sizeof(static_subnet));
  }
  
  if (doc["wifi"]["static_dns"].is<const char*>()) {
    strlcpy(static_dns, doc["wifi"]["static_dns"], sizeof(static_dns));
  }
  
  // Import MQTT Configuration
  if (doc["mqtt"]["server"].is<const char*>()) {
    strlcpy(mqtt_server, doc["mqtt"]["server"], sizeof(mqtt_server));
  }
  
  if (doc["mqtt"]["port"].is<int>()) {
    mqtt_port = doc["mqtt"]["port"];
  }
  
  if (doc["mqtt"]["user"].is<const char*>()) {
    strlcpy(mqtt_user, doc["mqtt"]["user"], sizeof(mqtt_user));
  }
  
  if (doc["mqtt"]["password"].is<const char*>()) {
    const char* pwd = doc["mqtt"]["password"];
    // Skip if placeholder
    if (strcmp(pwd, "***NOT_EXPORTED***") != 0) {
      strlcpy(mqtt_pass, pwd, sizeof(mqtt_pass));
    }
  }
  
  if (doc["mqtt"]["topic"].is<const char*>()) {
    strlcpy(mqtt_topic, doc["mqtt"]["topic"], sizeof(mqtt_topic));
  }
  
  // Import Gas Configuration
  if (doc["gas"]["calorific_value"].is<float>()) {
    gas_calorific_value = doc["gas"]["calorific_value"];
  }
  
  if (doc["gas"]["correction_factor"].is<float>()) {
    gas_correction_factor = doc["gas"]["correction_factor"];
  }
  
  if (doc["gas"]["poll_interval_seconds"].is<int>()) {
    poll_interval = doc["gas"]["poll_interval_seconds"].as<unsigned long>() * 1000;
  }
  
  if (doc["gas"]["base_price_monthly"].is<float>()) {
    gas_base_price_monthly = doc["gas"]["base_price_monthly"];
  }
  
  if (doc["gas"]["work_price_per_kwh"].is<float>()) {
    gas_work_price_per_kwh = doc["gas"]["work_price_per_kwh"];
  }
  
  // Import Deep Sleep (optional)
  if (doc["deep_sleep"]["enabled"].is<bool>()) {
    enable_deep_sleep = doc["deep_sleep"]["enabled"];
  }
  
  if (doc["deep_sleep"]["duration_seconds"].is<int>()) {
    deep_sleep_duration = doc["deep_sleep"]["duration_seconds"];
  }
  
  // Validate and fix poll_interval
  validatePollInterval();
  
  // Generate availability topic
  snprintf(mqtt_availability_topic, sizeof(mqtt_availability_topic), 
           "%s_availability", mqtt_topic);
  
  Serial.println("✅ Config erfolgreich importiert aus JSON");
  return true;
}

bool Config::validateJson(const String& json, String* errorMsg) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, json);
  
  if (error) {
    if (errorMsg) *errorMsg = "Invalid JSON: " + String(error.c_str());
    return false;
  }
  
  // Check required fields
  if (!doc["wifi"].is<JsonObject>()) {
    if (errorMsg) *errorMsg = "Missing 'wifi' section";
    return false;
  }
  
  if (!doc["mqtt"].is<JsonObject>()) {
    if (errorMsg) *errorMsg = "Missing 'mqtt' section";
    return false;
  }
  
  if (!doc["gas"].is<JsonObject>()) {
    if (errorMsg) *errorMsg = "Missing 'gas' section";
    return false;
  }
  
  // Validate WiFi SSID (mandatory)
  if (!doc["wifi"]["ssid"].is<const char*>() || strlen(doc["wifi"]["ssid"]) == 0) {
    if (errorMsg) *errorMsg = "Invalid or missing WiFi SSID";
    return false;
  }
  
  // Validate MQTT Server (mandatory)
  if (!doc["mqtt"]["server"].is<const char*>() || strlen(doc["mqtt"]["server"]) == 0) {
    if (errorMsg) *errorMsg = "Invalid or missing MQTT server";
    return false;
  }
  
  // Validate MQTT Port
  if (doc["mqtt"]["port"].is<int>()) {
    int port = doc["mqtt"]["port"];
    if (port < 1 || port > 65535) {
      if (errorMsg) *errorMsg = "Invalid MQTT port (must be 1-65535)";
      return false;
    }
  }
  
  // Validate Gas Parameters
  if (doc["gas"]["calorific_value"].is<float>()) {
    float cal = doc["gas"]["calorific_value"];
    if (cal < 8.0 || cal > 13.0) {
      if (errorMsg) *errorMsg = "Invalid calorific value (must be 8.0-13.0)";
      return false;
    }
  }
  
  if (doc["gas"]["correction_factor"].is<float>()) {
    float corr = doc["gas"]["correction_factor"];
    if (corr < 0.9 || corr > 1.1) {
      if (errorMsg) *errorMsg = "Invalid correction factor (must be 0.9-1.1)";
      return false;
    }
  }
  
  if (doc["gas"]["poll_interval_seconds"].is<int>()) {
    int interval = doc["gas"]["poll_interval_seconds"];
    if (interval < 10 || interval > 300) {
      if (errorMsg) *errorMsg = "Invalid poll interval (must be 10-300 seconds)";
      return false;
    }
  }
  
  return true;
}

