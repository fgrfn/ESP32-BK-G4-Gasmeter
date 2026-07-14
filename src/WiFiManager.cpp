#include "WiFiManager.h"
#include <WiFi.h>
#include <esp_system.h>
#include "Config.h"
#include "Logger.h"
#include "constants.h"

DNSServer WiFiManager::dnsServer_;
bool WiFiManager::apMode_ = false;
char WiFiManager::apSsid_[33] = "";
char WiFiManager::apPassword_[17] = "";
uint32_t WiFiManager::lastReconnectAttempt_ = 0;

namespace {
void makeApPassword(char* out, size_t size) {
  static constexpr char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
  for (size_t i = 0; i + 1 < size; ++i) out[i] = alphabet[esp_random() % (sizeof(alphabet) - 1)];
  out[size - 1] = '\0';
}
}

void WiFiManager::begin(bool forceAccessPoint) {
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname(Config::hostname);
  if (forceAccessPoint || Config::ssid[0] == '\0') {
    startAccessPoint();
    return;
  }

  WiFi.mode(WIFI_STA);
  if (Config::staticIpEnabled) {
    IPAddress ip, gateway, subnet, dns;
    ip.fromString(Config::staticIp);
    gateway.fromString(Config::gateway);
    subnet.fromString(Config::subnet);
    dns.fromString(Config::dns);
    if (!WiFi.config(ip, gateway, subnet, dns)) Logger::warn("Static IP configuration failed");
  }
  WiFi.begin(Config::ssid, Config::wifiPassword);
  Logger::info("Connecting to Wi-Fi " + String(Config::ssid));
  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < Constants::WIFI_CONNECT_TIMEOUT_MS) delay(100);
  if (WiFi.status() != WL_CONNECTED) {
    Logger::warn("Wi-Fi timeout; enabling setup AP");
    startAccessPoint();
    return;
  }
  apMode_ = false;
  Logger::info("Wi-Fi connected: " + WiFi.localIP().toString());
}

void WiFiManager::startAccessPoint() {
  WiFi.disconnect(true, true);
  delay(100);
  WiFi.mode(WIFI_AP);
  uint8_t mac[6] = {};
  WiFi.softAPmacAddress(mac);
  snprintf(apSsid_, sizeof(apSsid_), "ESP32-Gas-%02X%02X%02X", mac[3], mac[4], mac[5]);
  makeApPassword(apPassword_, sizeof(apPassword_));
  WiFi.softAP(apSsid_, apPassword_);
  dnsServer_.start(53, "*", WiFi.softAPIP());
  apMode_ = true;
  Logger::warn("Setup AP active: " + String(apSsid_));
  Serial.printf("Setup AP password: %s\n", apPassword_);
  Serial.printf("Web user: %s\nWeb password: %s\nOTA password: %s\n", Config::webUser, Config::webPassword, Config::otaPassword);
}

void WiFiManager::loop() {
  if (apMode_) {
    dnsServer_.processNextRequest();
    return;
  }
  if (WiFi.status() == WL_CONNECTED) return;
  if (millis() - lastReconnectAttempt_ < 10000) return;
  lastReconnectAttempt_ = millis();
  reconnect();
}

void WiFiManager::reconnect() {
  Logger::warn("Wi-Fi reconnect");
  WiFi.disconnect();
  WiFi.begin(Config::ssid, Config::wifiPassword);
}

bool WiFiManager::connected() { return WiFi.status() == WL_CONNECTED; }
bool WiFiManager::accessPointMode() { return apMode_; }
const char* WiFiManager::accessPointSsid() { return apSsid_; }
const char* WiFiManager::accessPointPassword() { return apPassword_; }
String WiFiManager::ipAddress() { return apMode_ ? WiFi.softAPIP().toString() : WiFi.localIP().toString(); }
