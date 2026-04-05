#include "WiFiManager.h"
#include "Config.h"
#include "Logger.h"

// Static member initialization
bool WiFiManager::apMode = false;
unsigned long WiFiManager::apModeStartTime = 0;

void WiFiManager::init() {
  apMode = false;
}

bool WiFiManager::configureStaticIP() {
  if (!Config::use_static_ip) {
    return true;
  }
  
  IPAddress ip, gateway, subnet, dns;
  ip.fromString(Config::static_ip);
  gateway.fromString(Config::static_gateway);
  subnet.fromString(Config::static_subnet);
  dns.fromString(Config::static_dns);
  
  if (!WiFi.config(ip, gateway, subnet, dns)) {
    Serial.println("Static IP Konfiguration fehlgeschlagen!");
    return false;
  }
  
  char msg[60];
  snprintf(msg, sizeof(msg), "Static IP konfiguriert: %s", Config::static_ip);
  Serial.println(msg);
  return true;
}

bool WiFiManager::connect() {
  // Wenn SSID Default ist, direkt in AP-Modus gehen
  if (strcmp(Config::ssid, DEFAULT_SSID) == 0 || strlen(Config::ssid) == 0) {
    Serial.println("Keine WLAN-Konfiguration gefunden. Starte Access Point...");
    startAPMode();
    return false;
  }
  
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(Config::hostname);
  
  // Static IP konfigurieren falls aktiviert
  configureStaticIP();
  
  WiFi.begin(Config::ssid, Config::password);
  Serial.print("Verbinde mit WLAN: ");
  Serial.println(Config::ssid);
  
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Verbunden! IP: ");
    Serial.println(WiFi.localIP());
    char msg[60];
    snprintf(msg, sizeof(msg), "WiFi verbunden: %s", WiFi.localIP().toString().c_str());
    Logger::addLog(msg);
    apMode = false;
    return true;
  } else {
    Serial.println("WLAN-Verbindung fehlgeschlagen!");
    char msg[80];
    snprintf(msg, sizeof(msg), "WiFi: Verbindung zu %s fehlgeschlagen", Config::ssid);
    Logger::addLog(msg);
    Serial.println("Starte Access Point fuer Konfiguration...");
    Logger::addLog("Starte Access Point Modus");
    startAPMode();
    return false;
  }
}

void WiFiManager::startAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  apMode = true;
  apModeStartTime = millis();
  
  Serial.println("Access Point gestartet");
  Serial.print("SSID: ");
  Serial.println(AP_SSID);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());
  
  char msg[100];
  snprintf(msg, sizeof(msg), "AP-Modus: SSID=%s, IP=%s", 
           AP_SSID, WiFi.softAPIP().toString().c_str());
  Logger::addLog(msg);
}

void WiFiManager::checkConnection() {
  if (apMode) {
    return; // Im AP-Modus keine Reconnect-Versuche
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi Verbindung verloren, versuche Reconnect...");
    connect();
  }
}

String WiFiManager::getLocalIP() {
  if (apMode) {
    return WiFi.softAPIP().toString();
  }
  return WiFi.localIP().toString();
}

int WiFiManager::getRSSI() {
  if (apMode) {
    return 0;
  }
  return WiFi.RSSI();
}
