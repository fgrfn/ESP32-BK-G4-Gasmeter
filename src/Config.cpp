#include "Config.h"
#include <WiFi.h>
#include <esp_system.h>
#include <time.h>
#include "CoreLogic.h"
#include "Logger.h"

Preferences Config::preferences_;
char Config::ssid[33] = "";
char Config::wifiPassword[65] = "";
char Config::hostname[64] = "";
bool Config::staticIpEnabled = false;
char Config::staticIp[16] = "192.168.1.100";
char Config::gateway[16] = "192.168.1.1";
char Config::subnet[16] = "255.255.255.0";
char Config::dns[16] = "192.168.1.1";
char Config::mqttHost[65] = "";
uint16_t Config::mqttPort = 1883;
char Config::mqttUser[65] = "";
char Config::mqttPassword[65] = "";
char Config::mqttBaseTopic[65] = "gasmeter";
bool Config::mqttTls = false;
bool Config::mqttTlsInsecure = false;
String Config::mqttCaCert;
bool Config::mqttCommandsEnabled = false;
char Config::webUser[33] = "admin";
char Config::webPassword[65] = "";
char Config::otaPassword[65] = "";
char Config::deviceId[33] = "";
uint32_t Config::pollIntervalMs = Constants::DEFAULT_POLL_INTERVAL_MS;
float Config::calorificValue = Constants::DEFAULT_CALORIFIC_VALUE;
float Config::correctionFactor = Constants::DEFAULT_CORRECTION_FACTOR;
float Config::meterOffsetM3 = 0.0f;
float Config::maxFlowM3h = Constants::DEFAULT_MAX_FLOW_M3H;
char Config::timezone[65] = "CET-1CEST,M3.5.0,M10.5.0/3";
TariffPeriod Config::tariffs[4];
uint8_t Config::tariffCount = 1;

namespace {
void copyJsonString(JsonVariantConst value, char* target, size_t targetSize) {
  if (value.is<const char*>()) strlcpy(target, value.as<const char*>(), targetSize);
}

void randomSecret(char* out, size_t size) {
  static constexpr char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
  if (size < 2) return;
  for (size_t i = 0; i < size - 1; ++i) out[i] = alphabet[esp_random() % (sizeof(alphabet) - 1)];
  out[size - 1] = '\0';
}

bool parseDate(const char* value, time_t& result) {
  if (!value || strlen(value) != 10) return false;
  struct tm tmValue = {};
  if (sscanf(value, "%d-%d-%d", &tmValue.tm_year, &tmValue.tm_mon, &tmValue.tm_mday) != 3) return false;
  tmValue.tm_year -= 1900;
  tmValue.tm_mon -= 1;
  tmValue.tm_isdst = -1;
  result = mktime(&tmValue);
  return result > 0;
}
}

void Config::setDefaults() {
  ssid[0] = '\0';
  wifiPassword[0] = '\0';
  strlcpy(hostname, Constants::DEFAULT_HOSTNAME, sizeof(hostname));
  staticIpEnabled = false;
  strlcpy(staticIp, "192.168.1.100", sizeof(staticIp));
  strlcpy(gateway, "192.168.1.1", sizeof(gateway));
  strlcpy(subnet, "255.255.255.0", sizeof(subnet));
  strlcpy(dns, "192.168.1.1", sizeof(dns));
  mqttHost[0] = '\0';
  mqttPort = 1883;
  mqttUser[0] = '\0';
  mqttPassword[0] = '\0';
  strlcpy(mqttBaseTopic, Constants::DEFAULT_MQTT_TOPIC, sizeof(mqttBaseTopic));
  mqttTls = false;
  mqttTlsInsecure = false;
  mqttCaCert = "";
  mqttCommandsEnabled = false;
  strlcpy(webUser, "admin", sizeof(webUser));
  webPassword[0] = '\0';
  otaPassword[0] = '\0';
  pollIntervalMs = Constants::DEFAULT_POLL_INTERVAL_MS;
  calorificValue = Constants::DEFAULT_CALORIFIC_VALUE;
  correctionFactor = Constants::DEFAULT_CORRECTION_FACTOR;
  meterOffsetM3 = 0.0f;
  maxFlowM3h = Constants::DEFAULT_MAX_FLOW_M3H;
  strlcpy(timezone, Constants::DEFAULT_TIMEZONE, sizeof(timezone));
  tariffCount = 1;
  tariffs[0] = TariffPeriod{};
  for (uint8_t i = 1; i < 4; ++i) tariffs[i] = TariffPeriod{};
}

void Config::begin() {
  setDefaults();
  generateDeviceIdentity();
}

void Config::generateDeviceIdentity() {
  uint8_t mac[6] = {};
  WiFi.macAddress(mac);
  snprintf(deviceId, sizeof(deviceId), "esp32_gas_%02x%02x%02x", mac[3], mac[4], mac[5]);
}

void Config::ensureSecrets() {
  if (webPassword[0] == '\0') randomSecret(webPassword, 17);
  if (otaPassword[0] == '\0') randomSecret(otaPassword, 17);
}

void Config::migrate(uint32_t fromSchema) {
  if (fromSchema < 2) {
    pollIntervalMs = CoreLogic::clampPollIntervalMs(pollIntervalMs);
  }
  if (fromSchema < 3) {
    if (tariffCount == 0) tariffCount = 1;
    if (tariffs[0].validFrom[0] == '\0') strlcpy(tariffs[0].validFrom, "1970-01-01", sizeof(tariffs[0].validFrom));
  }
}

bool Config::load() {
  if (!preferences_.begin("gasmeter", false)) return false;
  const uint32_t schema = preferences_.getUInt("schema", 0);
  preferences_.getString("ssid", ssid, sizeof(ssid));
  preferences_.getString("wifi_pass", wifiPassword, sizeof(wifiPassword));
  preferences_.getString("hostname", hostname, sizeof(hostname));
  staticIpEnabled = preferences_.getBool("static_ip_en", false);
  preferences_.getString("static_ip", staticIp, sizeof(staticIp));
  preferences_.getString("gateway", gateway, sizeof(gateway));
  preferences_.getString("subnet", subnet, sizeof(subnet));
  preferences_.getString("dns", dns, sizeof(dns));
  preferences_.getString("mqtt_host", mqttHost, sizeof(mqttHost));
  mqttPort = preferences_.getUShort("mqtt_port", 1883);
  preferences_.getString("mqtt_user", mqttUser, sizeof(mqttUser));
  preferences_.getString("mqtt_pass", mqttPassword, sizeof(mqttPassword));
  preferences_.getString("mqtt_topic", mqttBaseTopic, sizeof(mqttBaseTopic));
  mqttTls = preferences_.getBool("mqtt_tls", false);
  mqttTlsInsecure = preferences_.getBool("mqtt_insec", false);
  mqttCaCert = preferences_.getString("mqtt_ca", "");
  mqttCommandsEnabled = preferences_.getBool("mqtt_cmd", false);
  preferences_.getString("web_user", webUser, sizeof(webUser));
  preferences_.getString("web_pass", webPassword, sizeof(webPassword));
  preferences_.getString("ota_pass", otaPassword, sizeof(otaPassword));
  pollIntervalMs = preferences_.getULong("poll_ms", Constants::DEFAULT_POLL_INTERVAL_MS);
  calorificValue = preferences_.getFloat("calorific", Constants::DEFAULT_CALORIFIC_VALUE);
  correctionFactor = preferences_.getFloat("correction", Constants::DEFAULT_CORRECTION_FACTOR);
  meterOffsetM3 = preferences_.getFloat("meter_offset", 0.0f);
  maxFlowM3h = preferences_.getFloat("max_flow", Constants::DEFAULT_MAX_FLOW_M3H);
  preferences_.getString("timezone", timezone, sizeof(timezone));
  tariffCount = preferences_.getUChar("tariff_count", 1);
  const String tariffJson = preferences_.getString("tariffs", "");
  preferences_.end();

  if (!tariffJson.isEmpty()) {
    JsonDocument doc;
    if (!deserializeJson(doc, tariffJson)) {
      JsonArrayConst items = doc.as<JsonArrayConst>();
      tariffCount = static_cast<uint8_t>(items.size() < 4 ? items.size() : 4);
      for (uint8_t i = 0; i < tariffCount; ++i) {
        copyJsonString(items[i]["valid_from"], tariffs[i].validFrom, sizeof(tariffs[i].validFrom));
        tariffs[i].workPricePerKwh = items[i]["work_price_per_kwh"] | 0.12f;
        tariffs[i].basePriceMonthly = items[i]["base_price_monthly"] | 10.0f;
      }
    }
  }

  // Import legacy namespace once when upgrading from the previous firmware.
  if (schema == 0 && ssid[0] == '\0' && preferences_.begin("gas-config", true)) {
    preferences_.getString("ssid", ssid, sizeof(ssid));
    preferences_.getString("password", wifiPassword, sizeof(wifiPassword));
    preferences_.getString("hostname", hostname, sizeof(hostname));
    preferences_.getString("mqtt_server", mqttHost, sizeof(mqttHost));
    mqttPort = preferences_.getInt("mqtt_port", 1883);
    preferences_.getString("mqtt_user", mqttUser, sizeof(mqttUser));
    preferences_.getString("mqtt_pass", mqttPassword, sizeof(mqttPassword));
    preferences_.getString("mqtt_topic", mqttBaseTopic, sizeof(mqttBaseTopic));
    pollIntervalMs = preferences_.getULong("poll_interval", Constants::DEFAULT_POLL_INTERVAL_MS);
    calorificValue = preferences_.getFloat("gas_calorific", Constants::DEFAULT_CALORIFIC_VALUE);
    correctionFactor = preferences_.getFloat("gas_correction", Constants::DEFAULT_CORRECTION_FACTOR);
    tariffs[0].basePriceMonthly = preferences_.getFloat("gas_base_price", 10.0f);
    tariffs[0].workPricePerKwh = preferences_.getFloat("gas_work_price", 0.12f);
    preferences_.end();
  }

  migrate(schema);
  ensureSecrets();
  pollIntervalMs = CoreLogic::clampPollIntervalMs(pollIntervalMs);
  String error;
  if (!validate(error)) {
    Logger::error("Config invalid: " + error);
    return false;
  }
  save();
  return true;
}

bool Config::save() {
  ensureSecrets();
  String error;
  if (!validate(error)) {
    Logger::error("Config not saved: " + error);
    return false;
  }
  JsonDocument tariffDoc;
  JsonArray array = tariffDoc.to<JsonArray>();
  for (uint8_t i = 0; i < tariffCount; ++i) {
    JsonObject item = array.add<JsonObject>();
    item["valid_from"] = tariffs[i].validFrom;
    item["work_price_per_kwh"] = tariffs[i].workPricePerKwh;
    item["base_price_monthly"] = tariffs[i].basePriceMonthly;
  }
  String tariffJson;
  serializeJson(tariffDoc, tariffJson);

  if (!preferences_.begin("gasmeter", false)) return false;
  preferences_.putUInt("schema", CONFIG_SCHEMA_VERSION);
  preferences_.putString("ssid", ssid);
  preferences_.putString("wifi_pass", wifiPassword);
  preferences_.putString("hostname", hostname);
  preferences_.putBool("static_ip_en", staticIpEnabled);
  preferences_.putString("static_ip", staticIp);
  preferences_.putString("gateway", gateway);
  preferences_.putString("subnet", subnet);
  preferences_.putString("dns", dns);
  preferences_.putString("mqtt_host", mqttHost);
  preferences_.putUShort("mqtt_port", mqttPort);
  preferences_.putString("mqtt_user", mqttUser);
  preferences_.putString("mqtt_pass", mqttPassword);
  preferences_.putString("mqtt_topic", mqttBaseTopic);
  preferences_.putBool("mqtt_tls", mqttTls);
  preferences_.putBool("mqtt_insec", mqttTlsInsecure);
  preferences_.putString("mqtt_ca", mqttCaCert);
  preferences_.putBool("mqtt_cmd", mqttCommandsEnabled);
  preferences_.putString("web_user", webUser);
  preferences_.putString("web_pass", webPassword);
  preferences_.putString("ota_pass", otaPassword);
  preferences_.putULong("poll_ms", pollIntervalMs);
  preferences_.putFloat("calorific", calorificValue);
  preferences_.putFloat("correction", correctionFactor);
  preferences_.putFloat("meter_offset", meterOffsetM3);
  preferences_.putFloat("max_flow", maxFlowM3h);
  preferences_.putString("timezone", timezone);
  preferences_.putUChar("tariff_count", tariffCount);
  preferences_.putString("tariffs", tariffJson);
  preferences_.end();
  return true;
}

bool Config::validIpv4(const char* value) {
  IPAddress address;
  return address.fromString(value);
}

bool Config::validate(String& error) {
  pollIntervalMs = CoreLogic::clampPollIntervalMs(pollIntervalMs);
  if (!CoreLogic::isValidHostname(hostname)) { error = "invalid hostname"; return false; }
  if (mqttPort == 0) { error = "invalid MQTT port"; return false; }
  if (calorificValue < 5.0f || calorificValue > 20.0f) { error = "invalid calorific value"; return false; }
  if (correctionFactor < 0.5f || correctionFactor > 1.5f) { error = "invalid correction factor"; return false; }
  if (maxFlowM3h <= 0.0f || maxFlowM3h > 100.0f) { error = "invalid max flow"; return false; }
  if (staticIpEnabled && (!validIpv4(staticIp) || !validIpv4(gateway) || !validIpv4(subnet) || !validIpv4(dns))) {
    error = "invalid static IPv4 settings";
    return false;
  }
  if (tariffCount == 0 || tariffCount > 4) { error = "invalid tariff count"; return false; }
  for (uint8_t i = 0; i < tariffCount; ++i) {
    time_t parsed;
    if (!parseDate(tariffs[i].validFrom, parsed)) { error = "invalid tariff date"; return false; }
    if (tariffs[i].workPricePerKwh < 0 || tariffs[i].workPricePerKwh > 5 || tariffs[i].basePriceMonthly < 0 || tariffs[i].basePriceMonthly > 1000) {
      error = "invalid tariff price";
      return false;
    }
  }
  return true;
}

bool Config::importJson(JsonVariantConst root, String& error) {
  if (!root.is<JsonObjectConst>()) { error = "root must be an object"; return false; }
  JsonObjectConst wifi = root["wifi"];
  copyJsonString(wifi["ssid"], ssid, sizeof(ssid));
  copyJsonString(wifi["password"], wifiPassword, sizeof(wifiPassword));
  copyJsonString(wifi["hostname"], hostname, sizeof(hostname));
  staticIpEnabled = wifi["static_ip_enabled"] | staticIpEnabled;
  copyJsonString(wifi["static_ip"], staticIp, sizeof(staticIp));
  copyJsonString(wifi["gateway"], gateway, sizeof(gateway));
  copyJsonString(wifi["subnet"], subnet, sizeof(subnet));
  copyJsonString(wifi["dns"], dns, sizeof(dns));

  JsonObjectConst mqtt = root["mqtt"];
  copyJsonString(mqtt["host"], mqttHost, sizeof(mqttHost));
  mqttPort = mqtt["port"] | mqttPort;
  copyJsonString(mqtt["user"], mqttUser, sizeof(mqttUser));
  copyJsonString(mqtt["password"], mqttPassword, sizeof(mqttPassword));
  copyJsonString(mqtt["base_topic"], mqttBaseTopic, sizeof(mqttBaseTopic));
  mqttTls = mqtt["tls"] | mqttTls;
  mqttTlsInsecure = mqtt["tls_insecure"] | mqttTlsInsecure;
  mqttCommandsEnabled = mqtt["commands_enabled"] | mqttCommandsEnabled;
  if (mqtt["ca_cert"].is<const char*>()) mqttCaCert = mqtt["ca_cert"].as<const char*>();

  JsonObjectConst gas = root["gas"];
  pollIntervalMs = (gas["poll_interval_seconds"] | (pollIntervalMs / 1000UL)) * 1000UL;
  calorificValue = gas["calorific_value"] | calorificValue;
  correctionFactor = gas["correction_factor"] | correctionFactor;
  meterOffsetM3 = gas["meter_offset_m3"] | meterOffsetM3;
  maxFlowM3h = gas["max_flow_m3h"] | maxFlowM3h;

  JsonObjectConst security = root["security"];
  copyJsonString(security["web_user"], webUser, sizeof(webUser));
  copyJsonString(security["web_password"], webPassword, sizeof(webPassword));
  copyJsonString(security["ota_password"], otaPassword, sizeof(otaPassword));
  copyJsonString(root["timezone"], timezone, sizeof(timezone));

  if (root["tariffs"].is<JsonArrayConst>()) {
    JsonArrayConst values = root["tariffs"];
    tariffCount = static_cast<uint8_t>(values.size() < 4 ? values.size() : 4);
    for (uint8_t i = 0; i < tariffCount; ++i) {
      copyJsonString(values[i]["valid_from"], tariffs[i].validFrom, sizeof(tariffs[i].validFrom));
      tariffs[i].workPricePerKwh = values[i]["work_price_per_kwh"] | tariffs[i].workPricePerKwh;
      tariffs[i].basePriceMonthly = values[i]["base_price_monthly"] | tariffs[i].basePriceMonthly;
    }
  }
  ensureSecrets();
  return validate(error);
}

void Config::toJson(JsonObject root, bool includeSecrets) {
  root["schema_version"] = CONFIG_SCHEMA_VERSION;
  root["firmware_version"] = FIRMWARE_VERSION;
  root["device_id"] = deviceId;
  root["timezone"] = timezone;
  JsonObject wifi = root["wifi"].to<JsonObject>();
  wifi["ssid"] = ssid;
  wifi["password"] = includeSecrets ? wifiPassword : "";
  wifi["hostname"] = hostname;
  wifi["static_ip_enabled"] = staticIpEnabled;
  wifi["static_ip"] = staticIp;
  wifi["gateway"] = gateway;
  wifi["subnet"] = subnet;
  wifi["dns"] = dns;
  JsonObject mqtt = root["mqtt"].to<JsonObject>();
  mqtt["host"] = mqttHost;
  mqtt["port"] = mqttPort;
  mqtt["user"] = mqttUser;
  mqtt["password"] = includeSecrets ? mqttPassword : "";
  mqtt["base_topic"] = mqttBaseTopic;
  mqtt["tls"] = mqttTls;
  mqtt["tls_insecure"] = mqttTlsInsecure;
  mqtt["commands_enabled"] = mqttCommandsEnabled;
  if (includeSecrets) mqtt["ca_cert"] = mqttCaCert;
  else mqtt["ca_cert"] = "";
  JsonObject gas = root["gas"].to<JsonObject>();
  gas["poll_interval_seconds"] = pollIntervalMs / 1000UL;
  gas["calorific_value"] = calorificValue;
  gas["correction_factor"] = correctionFactor;
  gas["meter_offset_m3"] = meterOffsetM3;
  gas["max_flow_m3h"] = maxFlowM3h;
  JsonObject security = root["security"].to<JsonObject>();
  security["web_user"] = webUser;
  security["web_password"] = includeSecrets ? webPassword : "";
  security["ota_password"] = includeSecrets ? otaPassword : "";
  JsonArray tariffArray = root["tariffs"].to<JsonArray>();
  for (uint8_t i = 0; i < tariffCount; ++i) {
    JsonObject item = tariffArray.add<JsonObject>();
    item["valid_from"] = tariffs[i].validFrom;
    item["work_price_per_kwh"] = tariffs[i].workPricePerKwh;
    item["base_price_monthly"] = tariffs[i].basePriceMonthly;
  }
}

const TariffPeriod& Config::activeTariff(time_t now) {
  uint8_t selected = 0;
  time_t selectedTime = 0;
  for (uint8_t i = 0; i < tariffCount; ++i) {
    time_t candidate;
    if (parseDate(tariffs[i].validFrom, candidate) && candidate <= now && candidate >= selectedTime) {
      selected = i;
      selectedTime = candidate;
    }
  }
  return tariffs[selected];
}

void Config::factoryReset() {
  preferences_.begin("gasmeter", false);
  preferences_.clear();
  preferences_.end();
  preferences_.begin("gas-config", false);
  preferences_.clear();
  preferences_.end();
}
