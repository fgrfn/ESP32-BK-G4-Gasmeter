#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <ESP32Ping.h>
#include <time.h>
#include <vector>

// ---- Firmware Version ----
const char* FIRMWARE_VERSION = "2.0.5";

// ---- ANSI Farb-Codes für Serial Monitor (deaktiviert für reine Text-Ausgabe) ----
#define ANSI_RESET   ""
#define ANSI_BOLD    ""
#define ANSI_RED     ""
#define ANSI_GREEN   ""
#define ANSI_YELLOW  ""
#define ANSI_BLUE    ""
#define ANSI_MAGENTA ""
#define ANSI_CYAN    ""
#define ANSI_WHITE   ""

// ---- Konfiguration ----
char ssid[32] = "SSID";
char password[64] = "Password";
char hostname[32] = "ESP32-GasZaehler"; // mDNS Hostname
char mqtt_server[64] = "192.168.178.1"; // MQTT Broker IP
int mqtt_port = 1883;
char mqtt_user[64] = ""; // MQTT Username (optional)
char mqtt_pass[64] = ""; // MQTT Password (optional)
char mqtt_topic[64] = "gaszaehler/verbrauch";
char mqtt_availability_topic[64] = "gaszaehler/availability";
char mqtt_client_id[32] = "ESP32GasClient";
unsigned long poll_interval = 30000; // Standard: 30 Sekunden
float gas_calorific_value = 10.0; // kWh/m - Brennwert (typisch 8-12 kWh/m)
float gas_correction_factor = 1.0; // Z-Zahl Korrekturfaktor (typisch 0.95-1.0)
bool use_static_ip = false;
char static_ip[16] = "192.168.1.100";
char static_gateway[16] = "192.168.1.1";
char static_subnet[16] = "255.255.255.0";
char static_dns[16] = "192.168.1.1";

// ---- Deep Sleep Konfiguration ----
bool enable_deep_sleep = false;
unsigned long deep_sleep_duration = 300; // Sekunden (5 Minuten)
unsigned long last_activity = 0;
const unsigned long INACTIVITY_TIMEOUT = 600000; // 10 Minuten keine Aktivitt

Preferences preferences;
Preferences historyPrefs;
bool haDiscoverySent = false;

// ---- WiFi AP Mode ----
bool apMode = false;
const char* ap_ssid = "ESP32-GasZaehler";
const char* ap_password = "12345678"; // Mindestens 8 Zeichen (bei Bedarf in Config ändern)
const unsigned long AP_MODE_TIMEOUT = 300000; // 5 Minuten im AP-Modus

// ---- Status LED ----
const int STATUS_LED_PIN = 2; // Onboard LED (GPIO2)
unsigned long lastLedBlink = 0;
bool ledState = false;

// ---- Config Reset Button ----
const int RESET_BUTTON_PIN = 0; // BOOT Button (GPIO0)

// ---- NTP Zeitserver ----
const char* ntpServer = "de.pool.ntp.org";
const long gmtOffset_sec = 3600; // UTC+1 (MEZ)
const int daylightOffset_sec = 3600; // Automatisch erkannt
bool timeInitialized = false;

// Automatische Sommerzeit-Erkennung fr Europa
bool isDST(time_t now) {
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  int month = timeinfo.tm_mon + 1;
  int day = timeinfo.tm_mday;
  int dow = timeinfo.tm_wday; // 0=Sonntag
  
  // Oktober bis Mrz: Winterzeit
  if (month < 3 || month > 10) return false;
  // April bis September: Sommerzeit
  if (month > 3 && month < 10) return true;
  
  // Mrz: Sommerzeit ab letztem Sonntag 2 Uhr
  if (month == 3) {
    int lastSunday = day - dow;
    while (lastSunday + 7 <= 31) lastSunday += 7;
    return day >= lastSunday;
  }
  
  // Oktober: Winterzeit ab letztem Sonntag 3 Uhr
  if (month == 10) {
    int lastSunday = day - dow;
    while (lastSunday + 7 <= 31) lastSunday += 7;
    return day < lastSunday;
  }
  
  return false;
}

// ---- Error Tracking ----
struct ErrorStats {
  unsigned long mbusTimeouts = 0;
  unsigned long mbusParseErrors = 0;
  unsigned long mqttErrors = 0;
  unsigned long wifiDisconnects = 0;
  unsigned long lastError = 0;
  char lastErrorMsg[64] = "";
};
ErrorStats errorStats;

// ---- M-Bus Statistics ----
struct MBusStats {
  unsigned long totalPolls = 0;
  unsigned long successfulPolls = 0;
  unsigned long totalResponseTime = 0;
  unsigned long lastResponseTime = 0;
  unsigned long avgResponseTime = 0;
  String lastHexDump = "";
};
MBusStats mbusStats;

String lastErrorMessage = "";

// ---- Live Log System ----
const int MAX_LOG_ENTRIES = 50;
struct LogEntry {
  unsigned long timestamp;
  String message;
};
std::vector<LogEntry> logBuffer;

void addLog(const String& msg) {
  // ANSI-Codes aus der Nachricht entfernen für WebUI (nur im logBuffer)
  String cleanMsg = msg;
  cleanMsg.replace("\033[0m", "");   // ANSI_RESET
  cleanMsg.replace("\033[1m", "");   // ANSI_BOLD
  cleanMsg.replace("\033[31m", "");  // ANSI_RED
  cleanMsg.replace("\033[32m", "");  // ANSI_GREEN
  cleanMsg.replace("\033[33m", "");  // ANSI_YELLOW
  cleanMsg.replace("\033[34m", "");  // ANSI_BLUE
  cleanMsg.replace("\033[35m", "");  // ANSI_MAGENTA
  cleanMsg.replace("\033[36m", "");  // ANSI_CYAN
  cleanMsg.replace("\033[37m", "");  // ANSI_WHITE
  
  LogEntry entry;
  entry.timestamp = millis();
  entry.message = cleanMsg;
  logBuffer.push_back(entry);
  
  // Ringbuffer: alte Einträge löschen
  if (logBuffer.size() > MAX_LOG_ENTRIES) {
    logBuffer.erase(logBuffer.begin());
  }
  
  // Serial Output mit echter Zeit wenn verfügbar (mit ANSI-Codes)
  if (timeInitialized) {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
    Serial.println(String("[") + timeStr + "] " + msg);
  } else {
    Serial.println("[" + String(millis()/1000) + "s] " + msg);
  }
}

WiFiClient espClient;
PubSubClient client(espClient);
WebServer server(80);
const size_t OTA_BUFFER_SIZE = 1460;

// ---- Verlaufsdaten ----
struct MeasurementData {
  unsigned long timestamp;
  float volume;
};
std::vector<MeasurementData> measurements;
const size_t MAX_MEASUREMENTS = 50;
float lastVolume = -1;
unsigned long lastMemoryCheck = 0;
const unsigned long MEMORY_CHECK_INTERVAL = 60000; // Jede Minute

// ---- M-Bus UART ----
HardwareSerial mbusSerial(1); // UART1
const int MBUS_RX_PIN = 16;   // GPIO16 (RX2) fr ESP32 DevKit V1
const int MBUS_TX_PIN = 17;   // GPIO17 (TX2) fr ESP32 DevKit V1
const long MBUS_BAUD = 2400;

// ---- MBUS State Maschine ----
enum MBusState { MBUS_IDLE, MBUS_WAIT_RESPONSE };
MBusState mbusState = MBUS_IDLE;
unsigned long mbusLastAction = 0;
const unsigned long MBUS_RESPONSE_TIMEOUT = 500; // ms

uint8_t mbusBuffer[256];
size_t mbusLen = 0;

// ---- Konfiguration laden/speichern ----
void loadConfig() {
  if (!preferences.begin("gas-config", false)) {
    Serial.println("ERROR: Konnte gas-config Namespace nicht oeffnen!");
    return;
  }
  
  // Prfen ob bereits konfiguriert wurde (config_done Flag)
  bool configDone = preferences.getBool("config_done", false);
  
  preferences.getString("ssid", ssid, sizeof(ssid));
  preferences.getString("password", password, sizeof(password));
  preferences.getString("hostname", hostname, sizeof(hostname));
  preferences.getString("mqtt_server", mqtt_server, sizeof(mqtt_server));
  mqtt_port = preferences.getInt("mqtt_port", 1883);
  preferences.getString("mqtt_user", mqtt_user, sizeof(mqtt_user));
  preferences.getString("mqtt_pass", mqtt_pass, sizeof(mqtt_pass));
  preferences.getString("mqtt_topic", mqtt_topic, sizeof(mqtt_topic));
  poll_interval = preferences.getULong("poll_interval", 30000);
  Serial.println("DEBUG loadConfig: poll_interval aus Flash = " + String(poll_interval) + " ms");
  gas_calorific_value = preferences.getFloat("gas_calorific", 10.0);
  gas_correction_factor = preferences.getFloat("gas_correction", 1.0);
  use_static_ip = preferences.getBool("use_static_ip", false);
  preferences.getString("static_ip", static_ip, sizeof(static_ip));
  preferences.getString("static_gateway", static_gateway, sizeof(static_gateway));
  preferences.getString("static_subnet", static_subnet, sizeof(static_subnet));
  preferences.getString("static_dns", static_dns, sizeof(static_dns));
  preferences.end();
  
  // Verlaufsdaten aus Flash laden
  // Erst versuchen mit read-write zu oeffnen um Namespace zu erstellen falls noetig
  if (!historyPrefs.begin("gas-history", false)) {
    Serial.println("WARN: Konnte gas-history Namespace nicht oeffnen/erstellen");
  }
  size_t dataCount = historyPrefs.getUInt("count", 0);
  
  // Validierung: Nicht mehr als MAX_MEASUREMENTS laden
  if (dataCount > MAX_MEASUREMENTS) {
    Serial.println("WARNUNG: Zu viele gespeicherte Daten (" + String(dataCount) + "), limitiere auf " + String(MAX_MEASUREMENTS));
    dataCount = MAX_MEASUREMENTS;
  }
  
  if (dataCount > 0) {
    Serial.println("Lade gespeicherte Verlaufsdaten: " + String(dataCount) + " Eintraege");
    measurements.clear(); // Sicherstellen dass leer
    
    for (size_t i = 0; i < dataCount; i++) {
      char key[16];
      
      snprintf(key, sizeof(key), "ts_%d", i);
      unsigned long timestamp = historyPrefs.getULong(key, 0);
      
      snprintf(key, sizeof(key), "vol_%d", i);
      float volume = historyPrefs.getFloat(key, 0.0);
      
      // Nur valide Werte laden
      if (timestamp > 0 && volume >= 0 && volume < 999999) {
        measurements.push_back({timestamp, volume});
      }
    }
    
    Serial.println("Erfolgreich " + String(measurements.size()) + " Messwerte geladen");
  }
  historyPrefs.end();
  
  // Validierung: Poll-Intervall muss zwischen 10s und 5min liegen.
  // Wenn im Flash ein ungültiger (z.B. 0) Wert gespeichert wurde, fallback auf 30s.
  Serial.println("DEBUG loadConfig: poll_interval vor Validierung = " + String(poll_interval) + " ms");
  if (poll_interval < 10000) {
    Serial.println("WARN: Ungueltiger poll_interval im Flash: " + String(poll_interval) + " ms - setze auf Default 30000 ms");
    poll_interval = 30000; // Fallback auf 30s statt 10s, um unerwartete 10s-Reset zu vermeiden
  }
  if (poll_interval > 300000) poll_interval = 300000; // Maximum 5min
  Serial.println("DEBUG loadConfig: poll_interval nach Validierung = " + String(poll_interval) + " ms");
  
  // Wenn noch nie konfiguriert oder SSID leer -> Defaults setzen
  if (!configDone || strlen(ssid) == 0) {
    Serial.println("Keine gltige Konfiguration gefunden - verwende Defaults");
    strcpy(ssid, "SSID");
    strcpy(password, "Password");
  }
  
  // Fallback auf Defaults wenn leer
  if (strlen(hostname) == 0) strcpy(hostname, "ESP32-GasZaehler");
  if (strlen(mqtt_server) == 0) strcpy(mqtt_server, "192.168.178.1");
  if (strlen(mqtt_topic) == 0) strcpy(mqtt_topic, "gaszaehler/verbrauch");
  
  // Availability Topic generieren
  snprintf(mqtt_availability_topic, sizeof(mqtt_availability_topic), "%s_availability", mqtt_topic);
}

void saveConfig() {
  // Validierung vor dem Speichern
  Serial.println("DEBUG saveConfig: poll_interval vor Validierung = " + String(poll_interval) + " ms");
  if (poll_interval < 10000) poll_interval = 10000;
  if (poll_interval > 300000) poll_interval = 300000; // Max 5min
  Serial.println("DEBUG saveConfig: poll_interval nach Validierung = " + String(poll_interval) + " ms");
  
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
  // Readback of poll_interval before end()
  unsigned long rb_after = preferences.getULong("poll_interval", 0);
  preferences.putFloat("gas_calorific", gas_calorific_value);
  preferences.putFloat("gas_correction", gas_correction_factor);
  preferences.putBool("use_static_ip", use_static_ip);
  preferences.putString("static_ip", static_ip);
  preferences.putString("static_gateway", static_gateway);
  preferences.putString("static_subnet", static_subnet);
  preferences.putString("static_dns", static_dns);
  preferences.putBool("config_done", true); // Markiere als konfiguriert
  preferences.end();

  Serial.println("Konfiguration gespeichert");
  Serial.println("Poll-Intervall: " + String(poll_interval / 1000) + "s (" + String(poll_interval) + "ms) saved_readback=" + String(rb_after) + " ms");
}

// ---- Persistent Data Storage ----
void saveHistory() {
  historyPrefs.begin("gas-history", false);
  
  // Alte Daten lschen
  historyPrefs.clear();
  
  // Anzahl speichern
  size_t saveCount = min(measurements.size(), (size_t)MAX_MEASUREMENTS);
  historyPrefs.putUInt("count", saveCount);
  
  // Neueste Messwerte speichern
  for (size_t i = 0; i < saveCount; i++) {
    size_t idx = measurements.size() - saveCount + i; // Die neuesten X Werte
    char key[16];
    
    snprintf(key, sizeof(key), "ts_%d", i);
    historyPrefs.putULong(key, measurements[idx].timestamp);
    
    snprintf(key, sizeof(key), "vol_%d", i);
    historyPrefs.putFloat(key, measurements[idx].volume);
  }
  
  historyPrefs.end();
  Serial.println("History gespeichert: " + String(saveCount) + " Eintraege");
}

// ---- Fehler loggen ----
void logError(const char* msg) {
  errorStats.lastError = millis();
  strncpy(errorStats.lastErrorMsg, msg, sizeof(errorStats.lastErrorMsg) - 1);
  errorStats.lastErrorMsg[sizeof(errorStats.lastErrorMsg) - 1] = '\0';
  Serial.print("ERROR: ");
  Serial.println(msg);
}

// ---- Memory Leak Prevention ----
void checkMemory() {
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t minFreeHeap = ESP.getMinFreeHeap();
  uint32_t heapSize = ESP.getHeapSize();
  
  // Warnung wenn weniger als 10KB frei
  if (freeHeap < 10240) {
    Serial.println("WARNUNG: Wenig freier Speicher: " + String(freeHeap) + " Bytes");
    
    // Notfall: Alte Logs lschen
    if (logBuffer.size() > 20) {
      size_t oldSize = logBuffer.size();
      logBuffer.erase(logBuffer.begin(), logBuffer.begin() + 10);
      Serial.println("Notfall-Cleanup: " + String(oldSize - logBuffer.size()) + " alte Logs gelscht");
    }
    
    // Notfall: Alte Messwerte lschen
    if (measurements.size() > 30 && freeHeap < 5120) {
      size_t oldSize = measurements.size();
      measurements.erase(measurements.begin(), measurements.begin() + 10);
      Serial.println("Notfall-Cleanup: " + String(oldSize - measurements.size()) + " alte Messwerte gelscht");
      // Flash auch aktualisieren
      saveHistory();
    }
  }
  
  //KRITISCH: Neustart wenn < 3KB
  if (freeHeap < 3072) {
    Serial.println("KRITISCH: Extrem wenig RAM! Speichere Daten und starte neu...");
    saveHistory();
    delay(1000);
    ESP.restart();
  }
  
  // Statistik ausgeben
  float heapUsage = ((heapSize - freeHeap) * 100.0) / heapSize;
  Serial.println("Heap: Free=" + String(freeHeap) + " Min=" + String(minFreeHeap) + 
                 " Usage=" + String(heapUsage, 1) + "% | " +
                 "Logs=" + String(logBuffer.size()) + " Measurements=" + String(measurements.size()));
}

// ---- Status LED ----
void updateStatusLED() {
  unsigned long now = millis();
  
  if (apMode) {
    // Sehr schnelles Blinken: AP-Modus aktiv
    if (now - lastLedBlink >= 100) {
      ledState = !ledState;
      digitalWrite(STATUS_LED_PIN, ledState ? HIGH : LOW);
      lastLedBlink = now;
    }
  } else if (WiFi.status() != WL_CONNECTED) {
    // Schnelles Blinken: WLAN Problem
    if (now - lastLedBlink >= 200) {
      ledState = !ledState;
      digitalWrite(STATUS_LED_PIN, ledState ? HIGH : LOW);
      lastLedBlink = now;
    }
  } else if (!client.connected()) {
    // Mittleres Blinken: MQTT Problem
    if (now - lastLedBlink >= 500) {
      ledState = !ledState;
      digitalWrite(STATUS_LED_PIN, ledState ? HIGH : LOW);
      lastLedBlink = now;
    }
  } else {
    // Langsames Blinken: Alles OK
    if (now - lastLedBlink >= 2000) {
      ledState = !ledState;
      digitalWrite(STATUS_LED_PIN, ledState ? HIGH : LOW);
      lastLedBlink = now;
    }
  }
}

// ---- Forward declarations ----
void startAPMode();

// ---- WLAN Setup ----
void setup_wifi() {
  // Wenn SSID "SSID" ist, direkt in AP-Modus gehen
  if (strcmp(ssid, "SSID") == 0 || strlen(ssid) == 0) {
    Serial.println("Keine WLAN-Konfiguration gefunden. Starte Access Point...");
    startAPMode();
    return;
  }
  
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostname);
  
  // Static IP konfigurieren falls aktiviert
  if (use_static_ip) {
    IPAddress ip, gateway, subnet, dns;
    ip.fromString(static_ip);
    gateway.fromString(static_gateway);
    subnet.fromString(static_subnet);
    dns.fromString(static_dns);
    if (!WiFi.config(ip, gateway, subnet, dns)) {
      Serial.println("Static IP Konfiguration fehlgeschlagen!");
    } else {
      Serial.println("Static IP konfiguriert: " + String(static_ip));
    }
  }
  
  WiFi.begin(ssid, password);
  Serial.print("Verbinde mit WLAN: ");
  Serial.println(ssid);
  
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Verbunden! IP: ");
    Serial.println(WiFi.localIP());
    addLog("WiFi verbunden: " + WiFi.localIP().toString());
    apMode = false;
  } else {
    Serial.println("WLAN-Verbindung fehlgeschlagen!");
    addLog("WiFi: Verbindung zu " + String(ssid) + " fehlgeschlagen");
    logError("WLAN Verbindung fehlgeschlagen");
    Serial.println("Starte Access Point fr Konfiguration...");
    addLog("Starte Access Point Modus");
    startAPMode();
  }
}

// ---- Access Point Modus starten ----
void startAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.println("\n========================================");
  Serial.println("   ACCESS POINT MODUS AKTIV");
  Serial.println("========================================");
  Serial.print("SSID: ");
  Serial.println(ap_ssid);
  Serial.println("Passwort: [gesetzt]");
  Serial.print("IP-Adresse: ");
  Serial.println(IP);
  Serial.println("\nVerbinden Sie sich mit dem Access Point");
  Serial.println("und ffnen Sie http://" + IP.toString());
  Serial.println("========================================\n");
  
  apMode = true;
}

// ---- MQTT Reconnect ----
void reconnect() {
  static unsigned long lastAttempt = 0;
  unsigned long now = millis();
  
  // Nur alle 5 Sekunden versuchen
  if (now - lastAttempt < 5000) {
    return;
  }
  
  lastAttempt = now;
  
  if (!client.connected()) {
    String authInfo = (strlen(mqtt_user) > 0) ? " (Auth: " + String(mqtt_user) + ")" : " (ohne Auth)";
    addLog("MQTT: Verbinde zu " + String(mqtt_server) + ":" + String(mqtt_port) + authInfo);
    
    // Last Will Testament fr automatische Offline-Erkennung
    bool connected = false;
    if (strlen(mqtt_user) > 0) {
      // Mit Authentifizierung
      connected = client.connect(mqtt_client_id, mqtt_user, mqtt_pass, mqtt_availability_topic, 1, true, "offline");
    } else {
      // Ohne Authentifizierung
      connected = client.connect(mqtt_client_id, mqtt_availability_topic, 1, true, "offline");
    }
    
    if (connected) {
      addLog("MQTT: Verbunden!");
      
      // Online Status senden
      client.publish(mqtt_availability_topic, "online", true);
      
      // Fehler-Counter zurcksetzen bei erfolgreicher Verbindung
      if (errorStats.mqttErrors > 0) {
        addLog("MQTT: Verbindung wiederhergestellt (" + String(errorStats.mqttErrors) + " vorherige Fehler)");
        errorStats.mqttErrors = 0; // Counter zurücksetzen
      }
      
      haDiscoverySent = false; // Discovery neu senden nach Reconnect
    } else {
      String errMsg = "MQTT: Fehler rc=" + String(client.state());
      if (client.state() == 5) errMsg += " (Authentifizierung fehlgeschlagen)";
      addLog(errMsg);
      errorStats.mqttErrors++;
      logError("MQTT Verbindung fehlgeschlagen");
    }
  }
}

// ---- Home Assistant Auto-Discovery ----
void sendHomeAssistantDiscovery() {
  if (haDiscoverySent) return;
  
  // Alte Entities mit falscher Schreibweise löschen (gaszahler ohne "e")
  client.publish("homeassistant/sensor/gaszahler_gasverbrauch/config", "", true);
  client.publish("homeassistant/sensor/gaszahler_zahlerstand/config", "", true);
  client.publish("homeassistant/sensor/gaszahler_wifi/config", "", true);
  client.publish("homeassistant/sensor/gaszahler_m_bus_rate/config", "", true);
  client.publish("homeassistant/binary_sensor/gaszahler_online/config", "", true);
  delay(300);
  
  String dev = "{\"ids\":[\"esp32_gas\"],\"name\":\"Gaszähler\",\"mdl\":\"BK-G4\",\"mf\":\"ESP32\",\"sw\":\"" + String(FIRMWARE_VERSION) + "\"}";
  
  // 1. Gas Volume (m³ auf mqtt_topic)
  String p1 = "{\"name\":\"Zählerstand\",\"stat_t\":\"" + String(mqtt_topic) + "\",\"avty_t\":\"" + String(mqtt_availability_topic) + "\",\"unit_of_meas\":\"m³\",\"dev_cla\":\"gas\",\"stat_cla\":\"total_increasing\",\"val_tpl\":\"{{ value|float }}\",\"uniq_id\":\"esp32_gaszaehler_zaehlerstand\",\"dev\":" + dev + "}";
  client.publish("homeassistant/sensor/esp32_gaszaehler_zaehlerstand/config", p1.c_str(), true);
  delay(100);
  
  // 2. Energy (kWh auf mqtt_topic_energy)
  String p2 = "{\"name\":\"Gasverbrauch\",\"stat_t\":\"" + String(mqtt_topic) + "_energy\",\"avty_t\":\"" + String(mqtt_availability_topic) + "\",\"unit_of_meas\":\"kWh\",\"dev_cla\":\"energy\",\"stat_cla\":\"total_increasing\",\"val_tpl\":\"{{ value|float }}\",\"uniq_id\":\"esp32_gaszaehler_gasverbrauch\",\"dev\":" + dev + "}";
  client.publish("homeassistant/sensor/esp32_gaszaehler_gasverbrauch/config", p2.c_str(), true);
  delay(100);
  
  // 3. WiFi
  String p3 = "{\"name\":\"WiFi\",\"stat_t\":\"" + String(mqtt_topic) + "_wifi\",\"avty_t\":\"" + String(mqtt_availability_topic) + "\",\"unit_of_meas\":\"dBm\",\"dev_cla\":\"signal_strength\",\"val_tpl\":\"{{ value }}\",\"uniq_id\":\"esp32_gaszaehler_wifi\",\"dev\":" + dev + "}";
  client.publish("homeassistant/sensor/esp32_gaszaehler_wifi/config", p3.c_str(), true);
  delay(100);
  
  // 4. M-Bus Rate
  String p4 = "{\"name\":\"M-Bus Rate\",\"stat_t\":\"" + String(mqtt_topic) + "_mbus_rate\",\"avty_t\":\"" + String(mqtt_availability_topic) + "\",\"unit_of_meas\":\"%\",\"val_tpl\":\"{{ value }}\",\"ic\":\"mdi:check-network\",\"uniq_id\":\"esp32_gaszaehler_mbus\",\"dev\":" + dev + "}";
  client.publish("homeassistant/sensor/esp32_gaszaehler_mbus/config", p4.c_str(), true);
  delay(100);
  
  // 5. Online
  String p5 = "{\"name\":\"Online\",\"stat_t\":\"" + String(mqtt_availability_topic) + "\",\"pl_on\":\"online\",\"pl_off\":\"offline\",\"dev_cla\":\"connectivity\",\"uniq_id\":\"esp32_gaszaehler_online\",\"dev\":" + dev + "}";
  client.publish("homeassistant/binary_sensor/esp32_gaszaehler_online/config", p5.c_str(), true);
  
  Serial.println("HA Discovery gesendet (5 Entities)");
  Serial.println("Sensoren werden nach der ersten M-Bus Messung sichtbar!");
  Serial.println("Topics:");
  Serial.println("  - Volume: " + String(mqtt_topic));
  Serial.println("  - Energy: " + String(mqtt_topic) + "_energy");
  Serial.println("  - WiFi: " + String(mqtt_topic) + "_wifi");
  Serial.println("  - M-Bus Rate: " + String(mqtt_topic) + "_mbus_rate");
  Serial.println("  - Availability: " + String(mqtt_availability_topic));
  Serial.println("Brennwert: " + String(gas_calorific_value, 6) + " kWh/m³, Z-Zahl: " + String(gas_correction_factor, 6));
  haDiscoverySent = true;
}

// ---- BCD Parser fr Gaszhler ----
float parseGasVolumeBCD(const uint8_t* data, size_t len) {
    for (size_t i = 0; i + 5 < len; i++) {
        if (data[i] == 0x0C && data[i+1] == 0x13) { // DIF=0x0C, VIF=0x13
            uint32_t value = 0;
            uint32_t factor = 1;
            for (int b = 0; b < 4; b++) {
                uint8_t byte = data[i+2+b];
                uint8_t lsn = byte & 0x0F;
                uint8_t msn = (byte >> 4) & 0x0F;
                value += lsn * factor; factor *= 10;
                value += msn * factor; factor *= 10;
            }
            return value / 1000.0; // 2 Dezimalstellen  m
        }
    }
    return -1; // nicht gefunden
}

// ---- OTA Setup ----
void setupOTA() {
  ArduinoOTA.setHostname("esp32-gas");
  ArduinoOTA.onStart([]() { Serial.println("Start OTA Update"); });
  ArduinoOTA.onEnd([]() { Serial.println("\nOTA Ende"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Fortschritt: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Fehler[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth fehlgeschlagen");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin fehlgeschlagen");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Verbindung fehlgeschlagen");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Empfang fehlgeschlagen");
    else if (error == OTA_END_ERROR) Serial.println("End fehlgeschlagen");
  });
  ArduinoOTA.begin();
  Serial.println("OTA bereit");
}

// ---- WebServer Handler ----
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Gaszaehler Monitor</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/chartjs-adapter-date-fns@3.0.0/dist/chartjs-adapter-date-fns.bundle.min.js"></script>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    :root {
      --bg-gradient-start: #4f46e5;
      --bg-gradient-mid: #7c3aed;
      --bg-gradient-end: #2563eb;
      --card-bg: rgba(255, 255, 255, 0.95);
      --card-shadow: 0 20px 60px rgba(0, 0, 0, 0.15);
      --card-hover-shadow: 0 25px 70px rgba(0, 0, 0, 0.2);
      --text-primary: #1f2937;
      --text-secondary: #6b7280;
      --text-muted: #9ca3af;
      --border-color: rgba(229, 231, 235, 0.8);
      --input-bg: #ffffff;
      --input-focus-border: #4f46e5;
      --status-bg: #f9fafb;
      --accent-gradient: linear-gradient(135deg, #4f46e5 0%, #7c3aed 100%);
      --success-color: #10b981;
      --warning-color: #f59e0b;
      --error-color: #ef4444;
      --glass-bg: rgba(255, 255, 255, 0.1);
      --glass-border: rgba(255, 255, 255, 0.2);
    }
    body.dark-mode {
      --bg-gradient-start: #0f172a;
      --bg-gradient-mid: #1e1b4b;
      --bg-gradient-end: #1e293b;
      --card-bg: rgba(30, 41, 59, 0.9);
      --card-shadow: 0 20px 60px rgba(0, 0, 0, 0.4);
      --card-hover-shadow: 0 25px 70px rgba(0, 0, 0, 0.5);
      --text-primary: #f1f5f9;
      --text-secondary: #cbd5e1;
      --text-muted: #94a3b8;
      --border-color: rgba(71, 85, 105, 0.5);
      --input-bg: rgba(15, 23, 42, 0.6);
      --input-focus-border: #818cf8;
      --status-bg: rgba(15, 23, 42, 0.4);
      --glass-bg: rgba(30, 41, 59, 0.2);
      --glass-border: rgba(148, 163, 184, 0.1);
    }
    
    @keyframes fadeInUp {
      from {
        opacity: 0;
        transform: translateY(20px);
      }
      to {
        opacity: 1;
        transform: translateY(0);
      }
    }
    
    @keyframes pulse {
      0%, 100% { opacity: 1; }
      50% { opacity: 0.6; }
    }
    
    @keyframes slideIn {
      from {
        opacity: 0;
        transform: translateX(-20px);
      }
      to {
        opacity: 1;
        transform: translateX(0);
      }
    }
    
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Inter', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
      background: linear-gradient(135deg, var(--bg-gradient-start) 0%, var(--bg-gradient-mid) 50%, var(--bg-gradient-end) 100%);
      background-attachment: fixed;
      min-height: 100vh;
      padding: 20px;
      transition: all 0.4s cubic-bezier(0.4, 0, 0.2, 1);
      position: relative;
      overflow-x: hidden;
    }
    
    body::before {
      content: '';
      position: fixed;
      top: 0;
      left: 0;
      right: 0;
      bottom: 0;
      background: radial-gradient(circle at 20% 50%, rgba(99, 102, 241, 0.1) 0%, transparent 50%),
                  radial-gradient(circle at 80% 80%, rgba(168, 85, 247, 0.1) 0%, transparent 50%);
      pointer-events: none;
      z-index: 0;
    }
    .container {
      max-width: 1200px;
      margin: 0 auto;
      position: relative;
      z-index: 1;
    }
    
    .header {
      text-align: center;
      color: white;
      margin-bottom: 40px;
      animation: fadeInUp 0.6s ease-out;
    }
    
    .header h1 {
      font-size: 3em;
      font-weight: 800;
      margin-bottom: 8px;
      text-shadow: 0 4px 20px rgba(0,0,0,0.3);
      letter-spacing: -0.02em;
      color: #ffffff;
    }
    
    .header p {
      font-size: 1.1em;
      opacity: 0.95;
      font-weight: 500;
      letter-spacing: 0.02em;
      text-shadow: 0 2px 10px rgba(0,0,0,0.2);
    }
    
    .theme-toggle {
      position: absolute;
      top: 0;
      right: 0;
      background: var(--glass-bg);
      backdrop-filter: blur(20px);
      -webkit-backdrop-filter: blur(20px);
      border: 1px solid var(--glass-border);
      padding: 12px 20px;
      border-radius: 50px;
      color: white;
      cursor: pointer;
      font-size: 1.3em;
      transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
      box-shadow: 0 8px 24px rgba(0,0,0,0.15);
    }
    
    .theme-toggle:hover {
      transform: translateY(-2px) scale(1.05);
      box-shadow: 0 12px 32px rgba(0,0,0,0.2);
      background: var(--glass-border);
    }
    .nav {
      display: flex;
      gap: 12px;
      justify-content: center;
      margin-bottom: 30px;
      flex-wrap: wrap;
      animation: fadeInUp 0.6s ease-out 0.1s both;
    }
    
    .nav button {
      background: var(--glass-bg);
      backdrop-filter: blur(20px);
      -webkit-backdrop-filter: blur(20px);
      border: 1px solid var(--glass-border);
      color: white;
      padding: 14px 28px;
      border-radius: 12px;
      cursor: pointer;
      font-size: 0.95em;
      font-weight: 600;
      transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
      box-shadow: 0 4px 16px rgba(0,0,0,0.1);
      letter-spacing: 0.02em;
    }
    
    .nav button:hover {
      transform: translateY(-3px);
      box-shadow: 0 8px 24px rgba(0,0,0,0.2);
      background: var(--glass-border);
    }
    
    .nav button.active {
      background: linear-gradient(135deg, rgba(255,255,255,0.95) 0%, rgba(255,255,255,0.9) 100%);
      color: #4f46e5;
      box-shadow: 0 8px 28px rgba(79, 70, 229, 0.3);
      border-color: rgba(255,255,255,0.3);
    }
    
    .card {
      background: var(--card-bg);
      backdrop-filter: blur(20px);
      -webkit-backdrop-filter: blur(20px);
      border-radius: 20px;
      padding: 30px;
      margin-bottom: 24px;
      box-shadow: var(--card-shadow);
      border: 1px solid var(--glass-border);
      transition: all 0.4s cubic-bezier(0.4, 0, 0.2, 1);
      animation: fadeInUp 0.6s ease-out both;
    }
    
    .card:hover {
      box-shadow: var(--card-hover-shadow);
      transform: translateY(-4px);
    }
    
    .card h2 {
      color: var(--text-primary);
      font-size: 1.75em;
      font-weight: 700;
      margin-bottom: 20px;
      letter-spacing: -0.02em;
    }
    
    .card h3 {
      color: var(--text-primary);
      font-size: 1.3em;
      font-weight: 600;
      margin-bottom: 15px;
      margin-top: 25px;
    }
    .value-display {
      text-align: center;
      padding: 50px 40px;
      background: var(--accent-gradient);
      border-radius: 16px;
      color: white;
      box-shadow: 0 16px 48px rgba(79, 70, 229, 0.4);
      position: relative;
      overflow: hidden;
    }
    
    .value-display::before {
      content: '';
      position: absolute;
      top: -50%;
      left: -50%;
      width: 200%;
      height: 200%;
      background: radial-gradient(circle, rgba(255,255,255,0.1) 0%, transparent 70%);
      animation: pulse 3s ease-in-out infinite;
    }
    
    .value-display .label {
      font-size: 1.3em;
      opacity: 0.95;
      margin-bottom: 12px;
      font-weight: 600;
      letter-spacing: 0.05em;
      text-transform: uppercase;
      position: relative;
      z-index: 1;
    }
    
    .value-display .value {
      font-size: 4em;
      font-weight: 800;
      text-shadow: 0 4px 20px rgba(0,0,0,0.2);
      letter-spacing: -0.02em;
      position: relative;
      z-index: 1;
      line-height: 1;
    }
    .status-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
      gap: 16px;
      margin-top: 24px;
    }
    
    .status-item {
      padding: 20px;
      background: var(--status-bg);
      border-radius: 12px;
      border-left: 4px solid var(--input-focus-border);
      transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
      position: relative;
      overflow: hidden;
    }
    
    .status-item::before {
      content: '';
      position: absolute;
      top: 0;
      left: -100%;
      width: 100%;
      height: 100%;
      background: linear-gradient(90deg, transparent, rgba(79, 70, 229, 0.05), transparent);
      transition: left 0.5s ease;
    }
    
    .status-item:hover {
      transform: translateX(4px);
      box-shadow: 0 8px 24px rgba(0,0,0,0.08);
    }
    
    .status-item:hover::before {
      left: 100%;
    }
    
    .status-item .label {
      font-size: 0.85em;
      color: var(--text-secondary);
      margin-bottom: 8px;
      font-weight: 600;
      text-transform: uppercase;
      letter-spacing: 0.05em;
    }
    
    .status-item .value {
      font-size: 1.4em;
      font-weight: 700;
      color: var(--text-primary);
      letter-spacing: -0.01em;
    }
    
    .status-online { 
      border-left-color: var(--success-color);
      background: linear-gradient(135deg, rgba(16, 185, 129, 0.05) 0%, var(--status-bg) 100%);
    }
    .status-offline { 
      border-left-color: var(--error-color);
      background: linear-gradient(135deg, rgba(239, 68, 68, 0.05) 0%, var(--status-bg) 100%);
    }
    .chart-container {
      height: 300px;
      margin-top: 20px;
      position: relative;
      cursor: crosshair;
    }
    .chart {
      width: 100%;
      height: 100%;
      touch-action: pan-x pinch-zoom;
    }
    .chart-tooltip {
      position: absolute;
      background: rgba(0,0,0,0.8);
      color: white;
      padding: 8px 12px;
      border-radius: 6px;
      font-size: 0.85em;
      pointer-events: none;
      display: none;
      z-index: 1000;
      white-space: nowrap;
    }
    .form-group {
      margin-bottom: 24px;
    }
    
    .form-group label {
      display: block;
      margin-bottom: 8px;
      font-weight: 600;
      color: var(--text-primary);
      font-size: 0.95em;
      letter-spacing: 0.01em;
    }
    
    .form-group input, .form-group select {
      width: 100%;
      padding: 14px 16px;
      border: 2px solid var(--border-color);
      border-radius: 10px;
      font-size: 1em;
      background: var(--input-bg);
      color: var(--text-primary);
      transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
      font-family: inherit;
    }
    
    .form-group input:focus, .form-group select:focus {
      outline: none;
      border-color: var(--input-focus-border);
      box-shadow: 0 0 0 4px rgba(79, 70, 229, 0.1);
      transform: translateY(-1px);
    }
    
    .form-group small {
      display: block;
      margin-top: 6px;
      color: var(--text-secondary);
      font-size: 0.85em;
      line-height: 1.4;
    }
    
    .btn {
      background: var(--accent-gradient);
      color: white;
      border: none;
      padding: 14px 32px;
      border-radius: 10px;
      font-size: 1em;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
      box-shadow: 0 6px 20px rgba(79, 70, 229, 0.3);
      letter-spacing: 0.02em;
    }
    
    .btn:hover {
      transform: translateY(-2px);
      box-shadow: 0 10px 30px rgba(79, 70, 229, 0.4);
    }
    
    .btn:active {
      transform: translateY(0);
      box-shadow: 0 4px 15px rgba(79, 70, 229, 0.3);
    }
    
    .alert {
      padding: 16px 20px;
      border-radius: 12px;
      margin-bottom: 20px;
      display: none;
      font-weight: 500;
      border-left: 4px solid;
      animation: slideIn 0.3s ease-out;
    }
    
    .alert.success {
      background: rgba(16, 185, 129, 0.1);
      color: var(--success-color);
      border-color: var(--success-color);
    }
    
    .alert.error {
      background: rgba(239, 68, 68, 0.1);
      color: var(--error-color);
      border-color: var(--error-color);
    }
    
    .chart-container {
      height: 350px;
      margin-top: 24px;
      position: relative;
      cursor: crosshair;
      border-radius: 12px;
      overflow: hidden;
    }
    
    .chart {
      width: 100%;
      height: 100%;
      touch-action: pan-x pinch-zoom;
    }
    
    .chart-tooltip {
      position: absolute;
      background: rgba(15, 23, 42, 0.95);
      backdrop-filter: blur(10px);
      color: white;
      padding: 10px 14px;
      border-radius: 8px;
      font-size: 0.9em;
      pointer-events: none;
      display: none;
      z-index: 1000;
      white-space: nowrap;
      box-shadow: 0 8px 24px rgba(0,0,0,0.3);
      border: 1px solid rgba(255,255,255,0.1);
    }
    
    .page {
      display: none;
    }
    
    .page.active {
      display: block;
      animation: fadeInUp 0.4s ease-out;
    }
    
    @media (max-width: 768px) {
      .header h1 { font-size: 2.2em; }
      .value-display .value { font-size: 2.8em; }
      .value-display { padding: 40px 30px; }
      .nav { gap: 8px; }
      .nav button { padding: 12px 20px; font-size: 0.9em; }
      .card { padding: 20px; }
      .status-grid { grid-template-columns: 1fr; }
    }
    
    @media (max-width: 480px) {
      body { padding: 12px; }
      .header h1 { font-size: 1.8em; }
      .value-display .value { font-size: 2.2em; }
      .card h2 { font-size: 1.4em; }
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <button class="theme-toggle" id="themeToggle" onclick="toggleDarkMode()" title="Dark/Light Mode">🌙</button>
      <h1>⚡ Gaszähler Monitor</h1>
      <p>ESP32 M-Bus Gateway v%VERSION%</p>
      <div id="apModeWarning" style="display: none; background: #ff9800; color: white; padding: 10px; border-radius: 8px; margin-top: 10px;">
        ⚠️ <strong>Access Point Modus aktiv!</strong><br>
        Bitte konfigurieren Sie WLAN unter "Konfiguration" und speichern Sie die Einstellungen.
      </div>
    </div>

    <div class="nav">
      <button onclick="showPage('dashboard')" class="active" id="navDashboard">&#128200; Dashboard</button>
      <button onclick="showPage('config')" id="navConfig">&#9881; Konfiguration</button>
      <button onclick="showPage('logs')" id="navLogs">&#128221; Live Logs</button>
      <button onclick="showPage('diagnostics')" id="navDiagnostics">&#128295; Diagnose</button>
      <button onclick="showPage('update')" id="navUpdate">&#11014; Firmware Update</button>
    </div>

    <div id="dashboard" class="page active">
      <div class="card">
        <div class="value-display">
          <div class="label">&#128267; Aktueller Zählerstand</div>
          <div class="value" id="gasValue">-- m&sup3;</div>
          <div style="margin-top: 15px; font-size: 1em; opacity: 0.95; font-weight: 600;" id="energyValue">&#9889; -- kWh</div>
          <div style="margin-top: 20px; padding-top: 15px; border-top: 1px solid rgba(255,255,255,0.2); font-size: 0.85em; opacity: 0.9; display: flex; justify-content: center; gap: 20px;">
            <span>&#128293; Brennwert: <strong id="calorificDisplay">--</strong> kWh/m³</span>
            <span>&#9881; Zustandszahl: <strong id="correctionDisplay">--</strong></span>
          </div>
        </div>
      </div>

      <div class="card">
        <h2>&#128202; System Status</h2>
        <div class="status-grid">
          <div class="status-item" id="wifiStatus">
            <div class="label">&#128246; WLAN</div>
            <div class="value">--</div>
          </div>
          <div class="status-item" id="mqttStatus">
            <div class="label">&#128228; MQTT</div>
            <div class="value">--</div>
          </div>
          <div class="status-item">
            <div class="label">&#9201; Uptime</div>
            <div class="value" id="uptime">--</div>
          </div>
          <div class="status-item">
            <div class="label">&#128336; Letzte Messung</div>
            <div class="value" id="lastUpdate">--</div>
          </div>
          <div class="status-item">
            <div class="label">&#128337; Poll-Intervall</div>
            <div class="value" id="pollInterval">--</div>
          </div>
          <div class="status-item" id="wifiSignal">
            <div class="label">&#128246; WLAN Signal</div>
            <div class="value">--</div>
          </div>
        </div>
      </div>

      <div class="card">
        <h2>📊 M-Bus Statistik</h2>
        <div class="status-grid">
          <div class="status-item">
            <div class="label">✅ Erfolgsrate</div>
            <div class="value" id="mbusSuccessRate">--%</div>
          </div>
          <div class="status-item">
            <div class="label">📡 Abfragen Gesamt</div>
            <div class="value" id="mbusTotal">0</div>
          </div>
          <div class="status-item">
            <div class="label">⏱ Ø Antwortzeit</div>
            <div class="value" id="mbusAvgTime">-- ms</div>
          </div>
          <div class="status-item">
            <div class="label">⚡ Letzte Antwort</div>
            <div class="value" id="mbusLastTime">-- ms</div>
          </div>
        </div>
      </div>

      <div class="card">
        <h2 style="margin-bottom: 20px;">📈 Verlauf & Statistik</h2>
        
        <!-- Verbrauchsstatistik -->
        <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 12px; margin-bottom: 20px;">
          <div style="background: linear-gradient(135deg, rgba(16, 185, 129, 0.1) 0%, rgba(5, 150, 105, 0.1) 100%); border: 1px solid rgba(16, 185, 129, 0.3); border-radius: 10px; padding: 12px; text-align: center;">
            <div style="color: var(--text-muted); font-size: 0.85em; margin-bottom: 4px;">📅 Heute</div>
            <div style="color: #10b981; font-size: 1.3em; font-weight: bold;" id="statToday">-- m³</div>
            <div style="color: var(--text-muted); font-size: 0.75em;" id="statTodayKwh">-- kWh</div>
          </div>
          <div style="background: linear-gradient(135deg, rgba(59, 130, 246, 0.1) 0%, rgba(37, 99, 235, 0.1) 100%); border: 1px solid rgba(59, 130, 246, 0.3); border-radius: 10px; padding: 12px; text-align: center;">
            <div style="color: var(--text-muted); font-size: 0.85em; margin-bottom: 4px;">📆 Diese Woche</div>
            <div style="color: #3b82f6; font-size: 1.3em; font-weight: bold;" id="statWeek">-- m³</div>
            <div style="color: var(--text-muted); font-size: 0.75em;" id="statWeekKwh">-- kWh</div>
          </div>
          <div style="background: linear-gradient(135deg, rgba(139, 92, 246, 0.1) 0%, rgba(124, 58, 237, 0.1) 100%); border: 1px solid rgba(139, 92, 246, 0.3); border-radius: 10px; padding: 12px; text-align: center;">
            <div style="color: var(--text-muted); font-size: 0.85em; margin-bottom: 4px;">📅 Dieser Monat</div>
            <div style="color: #8b5cf6; font-size: 1.3em; font-weight: bold;" id="statMonth">-- m³</div>
            <div style="color: var(--text-muted); font-size: 0.75em;" id="statMonthKwh">-- kWh</div>
          </div>
          <div style="background: linear-gradient(135deg, rgba(251, 191, 36, 0.1) 0%, rgba(245, 158, 11, 0.1) 100%); border: 1px solid rgba(251, 191, 36, 0.3); border-radius: 10px; padding: 12px; text-align: center;">
            <div style="color: var(--text-muted); font-size: 0.85em; margin-bottom: 4px;">📊 Ø pro Tag</div>
            <div style="color: #fbbf24; font-size: 1.3em; font-weight: bold;" id="statAvg">-- m³</div>
            <div style="color: var(--text-muted); font-size: 0.75em;" id="statAvgKwh">-- kWh</div>
          </div>
        </div>
        
        <div class="chart-container" id="chartContainer">
          <canvas id="chart" class="chart"></canvas>
          <div id="chartTooltip" class="chart-tooltip"></div>
        </div>
      </div>
    </div>

    <div id="config" class="page">
      <div class="card">
        <h2>&#9881; Einstellungen</h2>
        <div id="configAlert" class="alert"></div>
        <form id="configForm" onsubmit="saveConfig(event)">
          <h3>&#128246; WLAN</h3>
          <div class="form-group">
            <label>SSID</label>
            <div style="display: flex; gap: 10px; align-items: start;">
              <input type="text" id="ssid" name="ssid" required style="flex: 1;" placeholder="Netzwerkname">
              <button type="button" onclick="scanWifi()" class="btn" style="padding: 12px 24px; white-space: nowrap;">&#128246; Scannen</button>
            </div>
            <select id="wifiList" onchange="selectWifi()" style="width: 100%; padding: 12px; border: 2px solid var(--border-color); border-radius: 10px; margin-top: 10px; display: none; background: var(--input-bg); color: var(--text-primary);">
              <option value="">-- Netzwerk auswhlen --</option>
            </select>
          </div>
          <div class="form-group">
            <label>Passwort</label>
            <div style="position: relative;">
              <input type="password" id="password" name="password" style="padding-right: 45px;">
              <button type="button" id="togglePassword" onclick="togglePasswordVisibility('password', 'togglePassword')" style="position: absolute; right: 10px; top: 50%; transform: translateY(-50%); background: none; border: none; cursor: pointer; font-size: 1.1em; color: var(--text-secondary); padding: 4px;">&#9680;</button>
            </div>
          </div>
          <div class="form-group">
            <label>Hostname</label>
            <input type="text" id="hostname" name="hostname" required>
            <small style="color: var(--text-secondary);">Für mDNS (z.B. ESP32-GasZaehler.local)</small>
          </div>
          
          <h3 style="margin-top: 30px;">Netzwerk-Einstellungen</h3>
          <div class="form-group">
            <label>
              <input type="checkbox" id="use_static_ip" name="use_static_ip" style="width: auto; margin-right: 10px;">
              Static IP verwenden (statt DHCP)
            </label>
          </div>
          <div id="staticIpFields" style="display: none;">
            <div class="form-group">
              <label>IP-Adresse</label>
              <input type="text" id="static_ip" name="static_ip" placeholder="192.168.1.100">
            </div>
            <div class="form-group">
              <label>Gateway</label>
              <input type="text" id="static_gateway" name="static_gateway" placeholder="192.168.1.1">
            </div>
            <div class="form-group">
              <label>Subnet Mask</label>
              <input type="text" id="static_subnet" name="static_subnet" placeholder="255.255.255.0">
            </div>
            <div class="form-group">
              <label>DNS Server</label>
              <input type="text" id="static_dns" name="static_dns" placeholder="192.168.1.1">
            </div>
          </div>
          
          <h3 style="margin-top: 30px;">MQTT</h3>
          <div class="form-group">
            <label>Server IP</label>
            <input type="text" id="mqtt_server" name="mqtt_server" required>
          </div>
          <div class="form-group">
            <label>Port</label>
            <input type="number" id="mqtt_port" name="mqtt_port" required>
          </div>
          <div class="form-group">
            <label>Benutzername (optional)</label>
            <input type="text" id="mqtt_user" name="mqtt_user">
            <small style="color: var(--text-secondary);">Leer lassen wenn keine Authentifizierung</small>
          </div>
          <div class="form-group">
            <label>Passwort (optional)</label>
            <div style="position: relative;">
              <input type="password" id="mqtt_pass" name="mqtt_pass" style="padding-right: 45px;">
              <button type="button" id="toggleMqttPass" onclick="togglePasswordVisibility('mqtt_pass', 'toggleMqttPass')" style="position: absolute; right: 10px; top: 50%; transform: translateY(-50%); background: none; border: none; cursor: pointer; font-size: 1.1em; color: var(--text-secondary); padding: 4px;">&#9680;</button>
            </div>
          </div>
          <div class="form-group">
            <label>Topic</label>
            <input type="text" id="mqtt_topic" name="mqtt_topic" required>
          </div>
          
          <h3 style="margin-top: 30px;">Abfrage-Einstellungen</h3>
          <div class="form-group">
            <label>Poll-Intervall (Sekunden)</label>
            <input type="number" id="poll_interval" name="poll_interval" min="10" max="300" required>
            <small style="color: #666;">Wie oft der Gaszähler abgefragt wird (10-300 Sekunden)</small>
          </div>
          
          <h3 style="margin-top: 30px;">Energie-Umrechnung (für Home Assistant Energy Dashboard)</h3>
          <div class="form-group">
            <label>Brennwert (kWh/m³)</label>
            <input type="number" id="gas_calorific" name="gas_calorific" step="0.000001" min="8" max="13" required>
            <small style="color: var(--text-muted);">Brennwert von Erdgas (typisch 10.0-10.5 kWh/m³, siehe Gasrechnung)</small>
          </div>
          <div class="form-group">
            <label>Korrekturfaktor (Z-Zahl)</label>
            <input type="number" id="gas_correction" name="gas_correction" step="0.000001" min="0.90" max="1.10" required>
            <small style="color: var(--text-muted);">Zustandszahl für Druck/Temperatur (typisch 0.95-0.97, siehe Gasrechnung)</small>
          </div>
          
          <button type="submit" class="btn" style="width: 100%; padding: 16px; font-size: 1.05em; margin-top: 10px;">&#128190; Speichern & Neustart</button>
        </form>
      </div>
    </div>

    <div id="logs" class="page">
      <div class="card">
        <h2>&#128221; Live Logs</h2>
        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; flex-wrap: wrap; gap: 10px;">
          <span style="color: var(--text-secondary); font-size: 0.95em;">&#128220; Zeigt die letzten 50 Log-Eintrage</span>
          <button onclick="refreshLogs()" class="btn" style="padding: 10px 20px;">&#128260; Aktualisieren</button>
        </div>
        <div id="logContainer" style="background: var(--status-bg); border-radius: 12px; padding: 20px; max-height: 600px; overflow-y: auto; font-family: 'Consolas', 'Monaco', monospace; font-size: 0.9em; line-height: 1.6; border: 1px solid var(--border-color); color: var(--text-primary);">
          <div style="text-align: center; color: var(--text-secondary); padding: 20px;">&#128260; Keine Logs vorhanden</div>
        </div>
      </div>
    </div>

    <div id="diagnostics" class="page">
      <div class="card">
        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 15px;">
          <h2 style="margin: 0;">⚠ Fehlerstatistik</h2>
          <button onclick="resetErrorStats()" class="btn" style="padding: 8px 16px; font-size: 0.9em; background: var(--warning-color);">🗑 Zurücksetzen</button>
        </div>
        <div class="status-grid">
          <div class="status-item">
            <div class="label">M-Bus Timeouts</div>
            <div class="value" id="errMbusTimeout">0</div>
          </div>
          <div class="status-item">
            <div class="label">M-Bus Parse-Fehler</div>
            <div class="value" id="errMbusParse">0</div>
          </div>
          <div class="status-item">
            <div class="label">MQTT Fehler</div>
            <div class="value" id="errMqtt">0</div>
          </div>
          <div class="status-item">
            <div class="label">WLAN Trennungen</div>
            <div class="value" id="errWifi">0</div>
          </div>
        </div>
        <div id="lastError" style="margin-top: 15px; padding: 15px; background: var(--status-bg); border-radius: 10px; border-left: 4px solid var(--error-color); display: none;">
          <strong style="color: var(--text-primary);">&#128681; Letzter Fehler:</strong> <span id="lastErrorMsg" style="color: var(--text-secondary); margin-left: 8px;">--</span>
        </div>
      </div>

      <div class="card">
        <h2>&#128187; System-Informationen</h2>
        <div class="status-grid">
          <div class="status-item">
            <div class="label">&#128190; Freier Heap</div>
            <div class="value" id="freeHeap">--</div>
          </div>
          <div class="status-item">
            <div class="label">&#128190; Heap-Größe</div>
            <div class="value" id="heapSize">--</div>
          </div>
          <div class="status-item">
            <div class="label">&#128191; Flash-Größe</div>
            <div class="value" id="flashSize">--</div>
          </div>
          <div class="status-item">
            <div class="label">&#128191; Sketch-Größe</div>
            <div class="value" id="sketchSize">--</div>
          </div>
          <div class="status-item">
            <div class="label">&#128191; Freier Flash</div>
            <div class="value" id="freeFlash">--</div>
          </div>
          <div class="status-item">
            <div class="label">&#9889; Chip-Modell</div>
            <div class="value" id="chipModel">--</div>
          </div>
        </div>
      </div>

      <div class="card">
        <h2>&#128295; Netzwerk-Diagnose</h2>
        <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 15px; margin-top: 24px;">
          <button onclick="testMQTT()" class="btn" style="padding: 18px; font-size: 1em;">&#128228; MQTT Verbindung testen</button>
          <button onclick="testWiFi()" class="btn" style="padding: 18px; font-size: 1em;">&#128246; WiFi Signal prüfen</button>
          <button onclick="pingGateway()" class="btn" style="padding: 18px; font-size: 1em;">&#127759; Gateway Ping</button>
          <button onclick="exportData()" class="btn" style="padding: 18px; font-size: 1em;">&#128190; Daten als CSV</button>
        </div>
        <div id="diagResult" style="margin-top: 24px; padding: 20px; background: var(--status-bg); border-radius: 12px; font-family: 'Consolas', 'Monaco', monospace; white-space: pre-wrap; display: none; border: 1px solid var(--border-color); line-height: 1.6;"></div>
      </div>

      <div class="card">
        <h2>&#128268; M-Bus Diagnose</h2>
        <div style="margin-bottom: 20px;">
          <button onclick="refreshMBusData()" class="btn" style="padding: 14px 28px;">&#128260; M-Bus Abfrage starten</button>
        </div>
        <div class="status-grid">
          <div class="status-item">
            <div class="label">Erfolgsquote</div>
            <div class="value" id="mbusSuccessRate">--</div>
          </div>
          <div class="status-item">
            <div class="label"> Antwortzeit</div>
            <div class="value" id="mbusAvgResponse">--</div>
          </div>
          <div class="status-item">
            <div class="label">Letzte Antwort</div>
            <div class="value" id="mbusLastResponse">--</div>
          </div>
          <div class="status-item">
            <div class="label">Gesamt Abfragen</div>
            <div class="value" id="mbusTotalPolls">--</div>
          </div>
        </div>
        <h3 style="margin-top: 20px;">Letzte M-Bus Rohdaten (Hex)</h3>
        <div id="mbusHexDump" style="background: var(--status-bg); border-radius: 8px; padding: 15px; font-family: monospace; font-size: 0.85em; word-break: break-all; color: var(--text-secondary);">Keine Daten vorhanden</div>
      </div>

      <div class="card">
        <h2> Letzte erfolgreiche Messungen</h2>
        <div id="lastMeasurements" style="max-height: 300px; overflow-y: auto;">
          <div style="text-align: center; color: var(--text-muted); padding: 20px;">Lade Daten...</div>
        </div>
      </div>

    </div>

    <div id="update" class="page">
      <div class="card">
        <h2>&#11014; Firmware Update</h2>
        
        <div style="background: linear-gradient(135deg, rgba(99, 102, 241, 0.1) 0%, rgba(168, 85, 247, 0.1) 100%); border: 2px solid #818cf8; border-radius: 12px; padding: 20px; margin-bottom: 24px;">
          <strong style="color: #818cf8; font-size: 1.1em;">&#9889; OTA Update über PlatformIO (empfohlen)</strong>
          <p style="margin: 12px 0; color: var(--text-primary); line-height: 1.6;">
            Der ESP32 unterstützt drahtloses Firmware-Update über das Netzwerk.
          </p>
          
          <div style="background: var(--status-bg); border-radius: 8px; padding: 16px; margin: 16px 0; font-family: 'Courier New', monospace; font-size: 0.9em;">
            <div style="color: var(--text-primary); margin-bottom: 8px; font-weight: 600;">1. In PlatformIO Terminal:</div>
            <code style="color: var(--text-primary); display: block; padding: 8px; background: var(--input-bg); border: 1px solid var(--border-color); border-radius: 4px; margin-bottom: 12px;">
              pio run -t upload --upload-port <span id="currentIP" style="color: #10b981; font-weight: bold;">Lädt...</span>
            </code>
            
            <div style="color: var(--text-primary); margin-bottom: 8px; font-weight: 600;">2. Oder in platformio.ini hinzufügen:</div>
            <code style="color: var(--text-primary); display: block; padding: 8px; background: var(--input-bg); border: 1px solid var(--border-color); border-radius: 4px; white-space: pre;">upload_protocol = espota
upload_port = <span id="currentIP2" style="color: #10b981; font-weight: bold;">Lädt...</span></code>
          </div>
        </div>

        <div style="background: linear-gradient(135deg, rgba(245, 158, 11, 0.1) 0%, rgba(251, 191, 36, 0.1) 100%); border: 2px solid #fbbf24; border-radius: 12px; padding: 20px; margin-bottom: 24px;">
          <strong style="color: #fbbf24; font-size: 1.1em;">&#9888; Alternative: USB-Upload</strong>
          <p style="margin: 12px 0 0 0; color: var(--text-primary); line-height: 1.6;">
            Bei Problemen mit OTA: ESP32 per USB verbinden und mit <code style="background: var(--status-bg); color: var(--text-primary); padding: 2px 6px; border-radius: 4px;">pio run -t upload</code> flashen.
          </p>
        </div>

        <div style="background: linear-gradient(135deg, rgba(99, 102, 241, 0.15) 0%, rgba(168, 85, 247, 0.15) 100%); border: 1px solid #818cf8; border-radius: 12px; padding: 20px; margin-top: 20px;">
          <div style="color: var(--text-primary); margin-bottom: 12px;"><strong style="color: #818cf8; font-size: 1.1em;">&#128218; Aktuell:</strong></div>
          <div style="color: var(--text-primary); line-height: 1.8;">
            <div style="margin-bottom: 8px;">Version: <code style="background: rgba(99, 102, 241, 0.2); color: #818cf8; padding: 4px 10px; border-radius: 6px; font-weight: bold;">%VERSION%</code></div>
            <div style="margin-bottom: 8px;">IP-Adresse: <code id="currentIP3" style="background: rgba(16, 185, 129, 0.2); color: #10b981; padding: 4px 10px; border-radius: 6px; font-weight: bold;">Lädt...</code></div>
            <div>Hostname: <code style="background: rgba(99, 102, 241, 0.2); color: #818cf8; padding: 4px 10px; border-radius: 6px; font-weight: bold;">esp32-gas.local</code></div>
          </div>
        </div>
      </div>
    </div>
  </div>

  <script>
    let currentPage = 'dashboard';
    let updateInterval = null;
    let fullHistoryData = [];

    // Dark Mode initialisieren
    function initDarkMode() {
      const darkMode = localStorage.getItem('darkMode') !== 'false'; // Default: true
      const themeBtn = document.getElementById('themeToggle');
      if (darkMode) {
        document.body.classList.add('dark-mode');
        if (themeBtn) themeBtn.textContent = '🌙';
        localStorage.setItem('darkMode', 'true');
      } else {
        if (themeBtn) themeBtn.textContent = '☀️';
      }
    }

    function toggleDarkMode() {
      const isDark = document.body.classList.toggle('dark-mode');
      localStorage.setItem('darkMode', isDark ? 'true' : 'false');
      const themeBtn = document.getElementById('themeToggle');
      if (themeBtn) themeBtn.textContent = isDark ? '🌙' : '☀️';
      
      // Chart neu zeichnen für Theme-Anpassung
      if (currentPage === 'dashboard' && fullHistoryData.length > 0) {
        drawChart(fullHistoryData);
      }
    }

    function togglePasswordVisibility(fieldId, buttonId) {
      const field = document.getElementById(fieldId);
      const button = document.getElementById(buttonId);
      if (field.type === 'password') {
        field.type = 'text';
        button.textContent = '\u25CB'; // Open circle
      } else {
        field.type = 'password';
        button.textContent = '\u25C0'; // Filled circle
      }
    }

    function showPage(page) {
      document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
      document.querySelectorAll('.nav button').forEach(b => b.classList.remove('active'));
      document.getElementById(page).classList.add('active');
      document.getElementById('nav' + page.charAt(0).toUpperCase() + page.slice(1)).classList.add('active');
      currentPage = page;
      
      if (page === 'dashboard') {
        if (updateInterval) clearInterval(updateInterval);
        updateData();
      } else {
        if (updateInterval) clearInterval(updateInterval);
        if (page === 'config') loadConfig();
        if (page === 'logs') {
          refreshLogs();
          updateInterval = setInterval(refreshLogs, 3000);
        }
        if (page === 'diagnostics') {
          refreshMBusData();
          fetch('/api/data').then(r => r.json()).then(data => {
            if (data.history && data.history.length > 0) {
              let html = '<div style="display: grid; gap: 10px;">';
              data.history.slice(-10).reverse().forEach(point => {
                const date = new Date(point.timestamp * 1000);
                const energy = (point.volume * data.calorific * data.correction).toFixed(3);
                html += `<div style="padding: 10px; background: var(--status-bg); border-radius: 5px; display: flex; justify-content: space-between;">`;
                html += `<span style="color: var(--text-secondary);">${date.toLocaleString('de-DE')}</span>`;
                html += `<strong style="color: var(--text-primary);">${point.volume.toFixed(2)} m³ / ${energy} kWh</strong>`;
                html += `</div>`;
              });
              html += '</div>';
              const el = document.getElementById('lastMeasurements');
              if (el) el.innerHTML = html;
            }
          });
        }
      }
    }

    function updateData() {
      fetch('/api/data')
        .then(r => r.json())
        .then(data => {
          const el = (id) => document.getElementById(id);
          
          // AP-Modus Warnung anzeigen
          if (data.apMode && el('apModeWarning')) {
            el('apModeWarning').style.display = 'block';
          }
          
          if (el('gasValue')) {
            el('gasValue').textContent = data.volume >= 0 ? data.volume.toFixed(2) + ' m³' : '-- m³';
          }
          
          // Energie-Anzeige
          if (el('energyValue')) {
            if (data.volume >= 0 && data.calorific > 0) {
              const energy = (data.volume * data.calorific * data.correction).toFixed(3);
              el('energyValue').textContent = '⚡ ' + energy + ' kWh';
            } else {
              el('energyValue').textContent = '⚡ -- kWh';
            }
          }
          
          // Brennwert & Zustandszahl anzeigen
          if (el('calorificDisplay')) {
            el('calorificDisplay').textContent = data.calorific > 0 ? data.calorific.toFixed(6) : '--';
          }
          if (el('correctionDisplay')) {
            el('correctionDisplay').textContent = data.correction > 0 ? data.correction.toFixed(6) : '--';
          }
          
          const wifiDiv = document.getElementById('wifiStatus');
          if (wifiDiv) {
            const valueEl = wifiDiv.querySelector('.value');
            if (valueEl) {
              if (data.apMode) {
                valueEl.textContent = 'AP-Modus (' + data.apSSID + ')';
                wifiDiv.className = 'status-item';
                wifiDiv.style.borderLeftColor = '#ff9800';
              } else {
                valueEl.textContent = data.wifiConnected ? 'Verbunden' : 'Getrennt';
                wifiDiv.className = data.wifiConnected ? 'status-item status-online' : 'status-item status-offline';
              }
            }
          }
          
          // WiFi Signal Strength
          const signalDiv = document.getElementById('wifiSignal');
          if (signalDiv) {
            const valueEl = signalDiv.querySelector('.value');
            if (valueEl) {
              if (data.wifiConnected && !data.apMode) {
                const rssi = data.wifiRSSI;
                let quality = 'Schlecht';
                let color = '#dc3545';
                let bars = '';
                
                if (rssi >= -50) {
                  quality = 'Ausgezeichnet';
                  color = '#28a745';
                  bars = '';
                } else if (rssi >= -60) {
                  quality = 'Gut';
                  color = '#28a745';
                  bars = '';
                } else if (rssi >= -70) {
                  quality = 'Mittel';
                  color = '#ffc107';
                  bars = '';
                } else if (rssi >= -80) {
                  quality = 'Schwach';
                  color = '#ff9800';
                  bars = '';
                }
                
                valueEl.textContent = bars + ' ' + rssi + ' dBm (' + quality + ')';
                signalDiv.style.borderLeftColor = color;
              } else {
                valueEl.textContent = '--';
                signalDiv.style.borderLeftColor = '#667eea';
              }
            }
          }
          
          const mqttDiv = document.getElementById('mqttStatus');
          if (mqttDiv) {
            const valueEl = mqttDiv.querySelector('.value');
            if (valueEl) {
              valueEl.textContent = data.mqttConnected ? 'Verbunden' : 'Getrennt';
              mqttDiv.className = data.mqttConnected ? 'status-item status-online' : 'status-item status-offline';
            }
          }
          
          if (el('uptime')) el('uptime').textContent = formatUptime(data.uptime);
          
          // Zeitstempel-Anzeige (NTP oder relative Zeit)
          if (el('lastUpdate')) {
            if (data.timeInitialized && data.lastUpdate > 1000000000) {
              const date = new Date(data.lastUpdate * 1000);
              el('lastUpdate').textContent = date.toLocaleTimeString('de-DE');
            } else {
              el('lastUpdate').textContent = 
                data.lastUpdate > 0 ? Math.floor((data.uptime - data.lastUpdate) / 1000) + 's' : '--';
            }
          }
          
          if (el('pollInterval')) el('pollInterval').textContent = data.pollInterval + 's';
          
          // IP-Adresse in Update-Seite eintragen
          const ipAddress = data.ipAddress || window.location.hostname || '10.10.40.109';
          const ipElements = ['currentIP', 'currentIP2', 'currentIP3'];
          ipElements.forEach(id => {
            const el = document.getElementById(id);
            if (el) el.textContent = ipAddress;
          });
          
          // System Info
          if (data.system) {
            const el = (id) => document.getElementById(id);
            if (el('freeHeap')) el('freeHeap').textContent = (data.system.freeHeap / 1024).toFixed(1) + ' KB';
            if (el('heapSize')) el('heapSize').textContent = (data.system.heapSize / 1024).toFixed(1) + ' KB';
            if (el('flashSize')) el('flashSize').textContent = (data.system.flashSize / 1024 / 1024).toFixed(1) + ' MB';
            if (el('sketchSize')) el('sketchSize').textContent = (data.system.sketchSize / 1024).toFixed(1) + ' KB';
            if (el('freeFlash')) el('freeFlash').textContent = (data.system.freeSketch / 1024).toFixed(1) + ' KB';
            if (el('chipModel')) el('chipModel').textContent = data.system.chipModel + ' (' + data.system.chipCores + ' Cores @ ' + data.system.cpuFreq + 'MHz)';
          }
          
          // M-Bus Statistiken (von /api/diagnostics)
          fetch('/api/diagnostics')
            .then(r => r.json())
            .then(mbusData => {
              if (mbusData.mbus) {
                const successRate = mbusData.mbus.total > 0 ? 
                  ((mbusData.mbus.successful / mbusData.mbus.total) * 100).toFixed(1) : '0.0';
                const el = (id) => document.getElementById(id);
                if (el('mbusSuccessRate')) el('mbusSuccessRate').textContent = successRate + '%';
                if (el('mbusTotal')) el('mbusTotal').textContent = mbusData.mbus.total;
                if (el('mbusAvgTime')) el('mbusAvgTime').textContent = 
                  mbusData.mbus.avgResponseTime > 0 ? mbusData.mbus.avgResponseTime + ' ms' : '-- ms';
                if (el('mbusLastTime')) el('mbusLastTime').textContent = 
                  mbusData.mbus.lastResponseTime > 0 ? mbusData.mbus.lastResponseTime + ' ms' : '-- ms';
              }
            })
            .catch(e => console.log('M-Bus Stats nicht verfügbar'));
          
          // Error Stats
          if (data.errors) {
            const el = (id) => document.getElementById(id);
            if (el('errMbusTimeout')) el('errMbusTimeout').textContent = data.errors.mbusTimeouts || 0;
            if (el('errMbusParse')) el('errMbusParse').textContent = data.errors.mbusParseErrors || 0;
            if (el('errMqtt')) el('errMqtt').textContent = data.errors.mqttErrors || 0;
            if (el('errWifi')) el('errWifi').textContent = data.errors.wifiDisconnects || 0;
          }
          
          // Letzter Fehler nur anzeigen wenn:
          // 1. Es einen Fehler gibt UND
          // 2. Der Fehler nicht älter als 2 Minuten ist UND
          // 3. System aktuell NICHT verbunden ist (sonst ist Fehler behoben)
          if (data.errors) {
            const totalErrors = (data.errors.mbusTimeouts || 0) + (data.errors.mbusParseErrors || 0) + 
                               (data.errors.mqttErrors || 0) + (data.errors.wifiDisconnects || 0);
            const errorAge = data.uptime - (data.errors.lastErrorTime || 0);
            const twoMinutes = 2 * 60 * 1000;
            const hasActiveIssue = !data.wifiConnected || !data.mqttConnected;
            
            const lastErrorEl = document.getElementById('lastError');
            if (lastErrorEl) {
              if (data.errors.lastError && totalErrors > 0 && errorAge < twoMinutes && hasActiveIssue) {
                lastErrorEl.style.display = 'block';
                const ageText = errorAge < 60000 ? 
                  'vor ' + Math.floor(errorAge / 1000) + 's' : 
                  'vor ' + Math.floor(errorAge / 60000) + 'min';
                const lastErrorMsg = document.getElementById('lastErrorMsg');
                if (lastErrorMsg) lastErrorMsg.textContent = data.errors.lastError + ' (' + ageText + ')';
              } else {
                lastErrorEl.style.display = 'none';
              }
            }
          }
          
          if (data.history && data.history.length > 0) {
            // Normalize timestamps: some stored timestamps may be in ms (old devices)
            // JS expects seconds. Detect and convert if needed.
            const nowMs = Date.now();
            let hist = data.history.map(p => ({ timestamp: p.timestamp, volume: p.volume }));
            const seemsMs = hist.some(p => p.timestamp > nowMs + 1000);
            if (seemsMs) {
              hist = hist.map(p => ({ timestamp: Math.floor(p.timestamp / 1000), volume: p.volume }));
            }
            fullHistoryData = hist;
            updateConsumptionStats(hist, data.calorific, data.correction);
            drawChart(hist);
          }
        })
        .catch(e => {
          console.error('API Fehler beim Laden der Daten:', e);
          // Zeige Fehler auf der Seite an
          const elements = ['wifiStatus', 'mqttStatus', 'gasValue'];
          elements.forEach(id => {
            const el = document.getElementById(id);
            if (el && el.querySelector) {
              const value = el.querySelector('.value');
              if (value) value.textContent = 'API Fehler';
            }
          });
        });
    }

    function updateConsumptionStats(history, calorific, correction) {
      const el = (id) => document.getElementById(id);
      
      if (!history || history.length < 2) {
        ['statToday', 'statWeek', 'statMonth', 'statAvg'].forEach(id => {
          if (el(id)) el(id).textContent = '-- m³';
          if (el(id + 'Kwh')) el(id + 'Kwh').textContent = '-- kWh';
        });
        return;
      }
      
      const now = Date.now() / 1000;
      const latest = history[history.length - 1];
      const oldest = history[0];
      
      // Start of today (00:00:00)
      const startOfToday = new Date();
      startOfToday.setHours(0, 0, 0, 0);
      const todayTimestamp = startOfToday.getTime() / 1000;
      
      // Start of this week (Monday 00:00:00)
      const startOfWeek = new Date();
      const dayOfWeek = startOfWeek.getDay(); // 0 = Sonntag, 1 = Montag
      const daysToMonday = dayOfWeek === 0 ? 6 : dayOfWeek - 1;
      startOfWeek.setDate(startOfWeek.getDate() - daysToMonday);
      startOfWeek.setHours(0, 0, 0, 0);
      const weekTimestamp = startOfWeek.getTime() / 1000;
      
      // Start of this month (1st day 00:00:00)
      const startOfMonth = new Date();
      startOfMonth.setDate(1);
      startOfMonth.setHours(0, 0, 0, 0);
      const monthTimestamp = startOfMonth.getTime() / 1000;
      
      // Finde den letzten Messwert VOR dem jeweiligen Zeitpunkt
      let todayStart = null;
      let weekStart = null;
      let monthStart = null;
      
      // Suche rückwärts nach dem letzten Wert vor dem Zeitpunkt
      for (let i = history.length - 1; i >= 0; i--) {
        if (!todayStart && history[i].timestamp < todayTimestamp) {
          todayStart = history[i];
        }
        if (!weekStart && history[i].timestamp < weekTimestamp) {
          weekStart = history[i];
        }
        if (!monthStart && history[i].timestamp < monthTimestamp) {
          monthStart = history[i];
        }
        if (todayStart && weekStart && monthStart) break;
      }
      
      // Falls kein Wert vor dem Zeitpunkt existiert, nutze den ersten verfügbaren
      if (!todayStart) todayStart = history[0];
      if (!weekStart) weekStart = history[0];
      if (!monthStart) monthStart = history[0];
      
      // Berechne Differenzen
      const todayDiff = latest.volume - todayStart.volume;
      const weekDiff = latest.volume - weekStart.volume;
      const monthDiff = latest.volume - monthStart.volume;
      
      // Durchschnitt pro Tag über die gesamte Historie
      const totalDiff = latest.volume - oldest.volume;
      const daysSinceStart = (latest.timestamp - oldest.timestamp) / 86400;
      const avgPerDay = daysSinceStart > 0 ? totalDiff / daysSinceStart : 0;
      
      // Anzeigen mit Null-Checks
      if (el('statToday')) el('statToday').textContent = todayDiff.toFixed(2) + ' m³';
      if (el('statTodayKwh')) el('statTodayKwh').textContent = (todayDiff * calorific * correction).toFixed(3) + ' kWh';
      
      if (el('statWeek')) el('statWeek').textContent = weekDiff.toFixed(2) + ' m³';
      if (el('statWeekKwh')) el('statWeekKwh').textContent = (weekDiff * calorific * correction).toFixed(3) + ' kWh';
      
      if (el('statMonth')) el('statMonth').textContent = monthDiff.toFixed(2) + ' m³';
      if (el('statMonthKwh')) el('statMonthKwh').textContent = (monthDiff * calorific * correction).toFixed(3) + ' kWh';
      
      if (el('statAvg')) el('statAvg').textContent = avgPerDay.toFixed(3) + ' m³';
      if (el('statAvgKwh')) el('statAvgKwh').textContent = (avgPerDay * calorific * correction).toFixed(3) + ' kWh';
    }

    function loadConfig() {
      fetch('/api/config')
        .then(r => r.json())
        .then(data => {
          const el = (id) => document.getElementById(id);
          
          if (el('ssid')) el('ssid').value = data.ssid;
          if (el('password')) el('password').value = data.password;
          if (el('hostname')) el('hostname').value = data.hostname || 'ESP32-GasZaehler';
          if (el('mqtt_server')) el('mqtt_server').value = data.mqtt_server;
          if (el('mqtt_port')) el('mqtt_port').value = data.mqtt_port;
          if (el('mqtt_user')) el('mqtt_user').value = data.mqtt_user || '';
          if (el('mqtt_pass')) el('mqtt_pass').value = data.mqtt_pass || '';
          if (el('mqtt_topic')) el('mqtt_topic').value = data.mqtt_topic;
          if (el('poll_interval')) el('poll_interval').value = data.poll_interval;
          if (el('gas_calorific')) el('gas_calorific').value = (data.gas_calorific || 10.0).toFixed(6);
          if (el('gas_correction')) el('gas_correction').value = (data.gas_correction || 1.0).toFixed(6);
          
          // Static IP Felder
          const useStaticIp = data.use_static_ip || false;
          if (el('use_static_ip')) el('use_static_ip').checked = useStaticIp;
          if (el('static_ip')) el('static_ip').value = data.static_ip || '192.168.1.100';
          if (el('static_gateway')) el('static_gateway').value = data.static_gateway || '192.168.1.1';
          if (el('static_subnet')) el('static_subnet').value = data.static_subnet || '255.255.255.0';
          if (el('static_dns')) el('static_dns').value = data.static_dns || '192.168.1.1';
          if (el('staticIpFields')) el('staticIpFields').style.display = useStaticIp ? 'block' : 'none';
          
          // Event Listener für Static IP Toggle
          if (el('use_static_ip')) {
            el('use_static_ip').addEventListener('change', function() {
              if (el('staticIpFields')) el('staticIpFields').style.display = this.checked ? 'block' : 'none';
            });
          }
        })
        .catch(e => console.error('Fehler:', e));
    }

    function refreshLogs() {
      fetch('/api/logs')
        .then(r => r.json())
        .then(data => {
          const container = document.getElementById('logContainer');
          if (!data.logs || data.logs.length === 0) {
            container.innerHTML = '<div style="text-align: center; color: var(--text-muted); padding: 20px;">Keine Logs verfügbar</div>';
            return;
          }
          
          let html = '';
          const now = Date.now();
          
          // Neueste zuerst
          for (let i = data.logs.length - 1; i >= 0; i--) {
            const log = data.logs[i];
            
            
            // Absolute Zeit berechnen (jetzt - uptime + log timestamp)
            const logDate = new Date(now - (data.uptime - log.timestamp));
            const timeStr = logDate.toLocaleTimeString('de-DE', {
              hour: '2-digit',
              minute: '2-digit',
              second: '2-digit'
            });
            
            // Farben für verschiedene Log-Typen
            let icon = '•';
            let color = 'var(--text-primary)';
            let category = '';
            const msg = log.message.toLowerCase();
            
            // Fehler & Warnungen
            if (msg.includes('fehler') || msg.includes('error') || msg.includes('failed')) {
              color = '#ef4444'; icon = '❌';
            } else if (msg.includes('warnung') || msg.includes('warning') || msg.includes('timeout')) {
              color = '#fbbf24'; icon = '⚠';
            } 
            // Erfolg
            else if (msg.includes('verbunden') || msg.includes('success') || msg.includes('ok') || msg.includes('erfolgreich')) {
              color = '#10b981'; icon = '✓';
            } 
            // Kategorien
            else if (msg.includes('m-bus')) {
              color = '#8b5cf6'; icon = '📡'; category = 'M-Bus';
            } else if (msg.includes('mqtt')) {
              color = '#06b6d4'; icon = '🔗'; category = 'MQTT';
            } else if (msg.includes('wifi') || msg.includes('wlan')) {
              color = '#3b82f6'; icon = '📶'; category = 'WiFi';
            } else if (msg.includes('start') || msg.includes('boot') || msg.includes('setup')) {
              color = '#3b82f6'; icon = '🚀';
            }
            
            html += `<div style="margin-bottom: 8px; padding: 8px; background: rgba(0,0,0,0.2); border-radius: 6px; border-left: 3px solid ${color};">`;
            html += `<span style="color: var(--text-secondary); font-weight: 600; font-size: 0.85em;">[${timeStr}]</span> `;
            
            html += `<span style="color: ${color}; margin: 0 4px;">${icon}</span>`;
            if (category) {
              html += `<span style="color: ${color}; font-weight: 600; font-size: 0.85em; background: rgba(0,0,0,0.3); padding: 2px 6px; border-radius: 3px; margin-right: 6px;">${category}</span>`;
            }
            html += `<span style="color: ${color};">${log.message}</span>`;
            html += '</div>';
          }
          container.innerHTML = html;
          
          // Auto-scroll nach unten wenn ntig
          if (container.scrollHeight - container.scrollTop < container.clientHeight + 100) {
            container.scrollTop = container.scrollHeight;
          }
        })
        .catch(e => console.error('Fehler:', e));
    }

    function scanWifi() {
      const btn = event.target;
      const originalText = btn.textContent;
      btn.textContent = 'Scanne...';
      btn.disabled = true;
      
      fetch('/api/wifi/scan')
        .then(r => r.json())
        .then(data => {
          const select = document.getElementById('wifiList');
          select.innerHTML = '<option value="">-- Netzwerk auswhlen --</option>';
          
          data.networks.forEach(net => {
            const option = document.createElement('option');
            const signal = net.rssi > -50 ? '' : net.rssi > -70 ? '' : '';
            const lock = net.encryption ? '' : '';
            option.value = net.ssid;
            option.textContent = `${net.ssid} ${signal} ${lock} (${net.rssi}dBm)`;
            select.appendChild(option);
          });
          
          select.style.display = 'block';
          btn.textContent = originalText;
          btn.disabled = false;
        })
        .catch(e => {
          console.error('Fehler:', e);
          alert('WiFi-Scan fehlgeschlagen!');
          btn.textContent = originalText;
          btn.disabled = false;
        });
    }

    function selectWifi() {
      const select = document.getElementById('wifiList');
      const ssidInput = document.getElementById('ssid');
      if (select.value) {
        ssidInput.value = select.value;
      }
    }

    function saveConfig(event) {
      event.preventDefault();
      const formData = new FormData(event.target);
      const config = {
        ssid: formData.get('ssid'),
        password: formData.get('password'),
        hostname: formData.get('hostname'),
        mqtt_server: formData.get('mqtt_server'),
        mqtt_port: parseInt(formData.get('mqtt_port')),
        mqtt_user: formData.get('mqtt_user'),
        mqtt_pass: formData.get('mqtt_pass'),
        mqtt_topic: formData.get('mqtt_topic'),
        // Ensure we send a valid integer: prefer parsed FormData, fallback to element value, then default 30
        poll_interval: (function(){
          const v = parseInt(formData.get('poll_interval'));
          if (!isNaN(v) && v > 0) return v;
          const el = document.getElementById('poll_interval');
          const ev = el ? parseInt(el.value) : NaN;
          if (!isNaN(ev) && ev > 0) return ev;
          return 30;
        })(),
        gas_calorific: parseFloat(formData.get('gas_calorific')),
        gas_correction: parseFloat(formData.get('gas_correction')),
        use_static_ip: document.getElementById('use_static_ip').checked,
        static_ip: formData.get('static_ip'),
        static_gateway: formData.get('static_gateway'),
        static_subnet: formData.get('static_subnet'),
        static_dns: formData.get('static_dns')
      };
      
      fetch('/api/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(config)
      })
      .then(r => r.json())
      .then(data => {
        const alert = document.getElementById('configAlert');
        alert.textContent = 'Einstellungen gespeichert! ESP32 startet neu...';
        alert.className = 'alert success';
        alert.style.display = 'block';
        setTimeout(() => { location.reload(); }, 3000);
      })
      .catch(e => {
        const alert = document.getElementById('configAlert');
        alert.textContent = 'Fehler beim Speichern!';
        alert.className = 'alert error';
        alert.style.display = 'block';
      });
    }

    function formatUptime(ms) {
      const s = Math.floor(ms / 1000);
      const m = Math.floor(s / 60);
      const h = Math.floor(m / 60);
      const d = Math.floor(h / 24);
      if (d > 0) return d + 'd ' + (h % 24) + 'h';
      if (h > 0) return h + 'h ' + (m % 60) + 'm';
      return m + 'm ' + (s % 60) + 's';
    }

    let myChart = null;
    
    function drawChart(history) {
      if (!history || history.length === 0) {
        const canvas = document.getElementById('chart');
        const ctx = canvas.getContext('2d');
        ctx.clearRect(0, 0, canvas.width, canvas.height);
        ctx.fillStyle = '#999';
        ctx.font = '16px sans-serif';
        ctx.textAlign = 'center';
        ctx.fillText('Keine Daten für diesen Zeitraum', canvas.width / 2, canvas.height / 2);
        if (myChart) {
          myChart.destroy();
          myChart = null;
        }
        return;
      }
      
      if (history.length < 2) return;
      
      // Daten für Chart.js aufbereiten
      const chartData = history.map(point => ({
        x: point.timestamp * 1000, // Chart.js benötigt Millisekunden
        y: point.volume
      }));
      
      // Min/Max für x-Achse berechnen
      const timestamps = chartData.map(d => d.x);
      const minTime = Math.min(...timestamps);
      const maxTime = Math.max(...timestamps);
      
      // Zeitachse-Einheit automatisch basierend auf Datenmenge
      const daysDiff = (maxTime - minTime) / (1000 * 60 * 60 * 24);
      let timeUnit = 'hour';
      let displayFormats = {hour: 'HH:mm'};
      let tooltipFormat = 'dd.MM HH:mm';
      let stepSize = undefined;
      let maxTicksLimit = undefined;
      
      if (daysDiff > 180) {
        timeUnit = 'month';
        displayFormats = {month: 'MMM yyyy'};
        tooltipFormat = 'MMM yyyy';
        maxTicksLimit = 20;
      } else if (daysDiff > 60) {
        timeUnit = 'week';
        displayFormats = {week: 'dd.MM'};
        tooltipFormat = 'dd.MM.yyyy';
        maxTicksLimit = 20;
      } else if (daysDiff > 14) {
        timeUnit = 'day';
        displayFormats = {day: 'dd.MM'};
        tooltipFormat = 'dd.MM.yyyy';
        maxTicksLimit = 30;
      } else if (daysDiff > 2) {
        timeUnit = 'hour';
        displayFormats = {hour: 'dd.MM HH:mm'};
        tooltipFormat = 'dd.MM HH:mm';
        maxTicksLimit = 25;
      } else {
        timeUnit = 'hour';
        displayFormats = {hour: 'HH:mm'};
        tooltipFormat = 'dd.MM HH:mm';
        maxTicksLimit = 48;
      }
      
      // Altes Chart zerstören
      if (myChart) {
        myChart.destroy();
      }
      
      const ctx = document.getElementById('chart').getContext('2d');
      myChart = new Chart(ctx, {
        type: 'line',
        data: {
          datasets: [{
            label: 'Gasverbrauch (m³)',
            data: chartData,
            borderColor: '#667eea',
            backgroundColor: 'rgba(102, 126, 234, 0.1)',
            borderWidth: 3,
            pointRadius: 4,
            pointHoverRadius: 6,
            pointBackgroundColor: '#667eea',
            pointBorderColor: '#fff',
            pointBorderWidth: 2,
            tension: 0.4,
            fill: true
          }]
        },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          interaction: {
            mode: 'nearest',
            axis: 'x',
            intersect: false
          },
          plugins: {
            legend: {
              display: true,
              labels: {
                color: '#666',
                font: {size: 14}
              }
            },
            tooltip: {
              callbacks: {
                title: function(context) {
                  const date = new Date(context[0].parsed.x);
                  return date.toLocaleString('de-DE', {
                    day: '2-digit',
                    month: '2-digit',
                    year: 'numeric',
                    hour: '2-digit',
                    minute: '2-digit'
                  });
                },
                label: function(context) {
                  return ' ' + context.parsed.y.toFixed(2) + ' m³';
                }
              }
            }
          },
          scales: {
            x: {
              type: 'time',
              time: {
                unit: timeUnit,
                displayFormats: displayFormats,
                tooltipFormat: tooltipFormat,
                stepSize: stepSize
              },
              min: minTime,
              max: maxTime,
              grid: {
                color: 'rgba(0, 0, 0, 0.05)'
              },
              ticks: {
                color: '#666',
                maxRotation: 45,
                minRotation: 0,
                autoSkip: true,
                maxTicksLimit: maxTicksLimit
              }
            },
            y: {
              beginAtZero: false,
              grid: {
                color: 'rgba(0, 0, 0, 0.05)'
              },
              ticks: {
                color: '#666',
                callback: function(value) {
                  return value.toFixed(2) + ' m³';
                }
              }
            }
          }
        });
      });
    }
    
    // Chart.js kümmert sich um Tooltips

    // Diagnose-Funktionen
    function testMQTT() {
      const result = document.getElementById('diagResult');
      result.style.display = 'block';
      result.style.color = 'var(--text-secondary)';
      result.innerHTML = '<span style="color: var(--accent-color);">⏳</span> Teste MQTT-Verbindung...';
      
      fetch('/api/test/mqtt')
        .then(r => r.json())
        .then(data => {
          const status = data.connected ? '<span style="color: #10b981;">✓ Verbunden</span>' : '<span style="color: #ef4444;">✗ Getrennt</span>';
          result.innerHTML = `<div style="color: var(--text-primary); line-height: 1.8;">
            <strong style="color: var(--accent-color); font-size: 1.1em;">📡 MQTT Test</strong><br><br>
            <span style="color: var(--text-secondary);">Server:</span> <code style="color: var(--accent-color);">${data.server}:${data.port}</code><br>
            <span style="color: var(--text-secondary);">Status:</span> ${status}<br>
            <span style="color: var(--text-secondary);">Last Will:</span> <code>${data.availability_topic}</code><br>
            <span style="color: var(--text-secondary);">Antwortzeit:</span> <strong style="color: #10b981;">${data.response_time}</strong>
          </div>`;
        })
        .catch(e => {
          result.innerHTML = '<span style="color: #ef4444;">❌ MQTT Test fehlgeschlagen: ' + e + '</span>';
        });
    }

    function testWiFi() {
      const result = document.getElementById('diagResult');
      result.style.display = 'block';
      result.style.color = 'var(--text-secondary)';
      result.innerHTML = '<span style="color: var(--accent-color);">⏳</span> Prüfe WiFi-Verbindung...';
      
      fetch('/api/test/wifi')
        .then(r => r.json())
        .then(data => {
          let quality = 'Schlecht';
          let qualityColor = '#ef4444';
          if (data.rssi >= -50) { quality = 'Ausgezeichnet'; qualityColor = '#10b981'; }
          else if (data.rssi >= -60) { quality = 'Gut'; qualityColor = '#10b981'; }
          else if (data.rssi >= -70) { quality = 'Mittel'; qualityColor = '#fbbf24'; }
          
          result.innerHTML = `<div style="color: var(--text-primary); line-height: 1.8;">
            <strong style="color: var(--accent-color); font-size: 1.1em;">📶 WiFi Status</strong><br><br>
            <span style="color: var(--text-secondary);">SSID:</span> <code style="color: var(--accent-color);">${data.ssid}</code><br>
            <span style="color: var(--text-secondary);">Signal:</span> <strong style="color: ${qualityColor};">${data.rssi} dBm (${quality})</strong><br>
            <span style="color: var(--text-secondary);">Kanal:</span> ${data.channel}<br>
            <span style="color: var(--text-secondary);">IP:</span> <code style="color: #10b981; font-weight: bold;">${data.ip}</code><br>
            <span style="color: var(--text-secondary);">MAC:</span> <code>${data.mac}</code><br>
            <span style="color: var(--text-secondary);">Hostname:</span> <code>${data.hostname}</code>
          </div>`;
        })
        .catch(e => {
          result.innerHTML = '<span style="color: #ef4444;">❌ WiFi Test fehlgeschlagen: ' + e + '</span>';
        });
    }

    function pingGateway() {
      const result = document.getElementById('diagResult');
      result.style.display = 'block';
      result.style.color = 'var(--text-secondary)';
      result.innerHTML = '<span style="color: var(--accent-color);">⏳</span> Pinge Gateway...';
      
      fetch('/api/test/ping')
        .then(r => r.json())
        .then(data => {
          const reachable = data.reachable ? '<span style="color: #10b981;">✓ Ja</span>' : '<span style="color: #ef4444;">✗ Nein</span>';
          result.innerHTML = `<div style="color: var(--text-primary); line-height: 1.8;">
            <strong style="color: var(--accent-color); font-size: 1.1em;">🌐 Gateway Ping</strong><br><br>
            <span style="color: var(--text-secondary);">Gateway:</span> <code style="color: var(--accent-color); font-weight: bold;">${data.gateway}</code><br>
            <span style="color: var(--text-secondary);">Erreichbar:</span> ${reachable}<br>
            <span style="color: var(--text-secondary);">Antwortzeit:</span> <strong style="color: #10b981;">${data.response_time}</strong><br>
            <span style="color: var(--text-secondary);">DNS:</span> <code>${data.dns}</code>
          </div>`;
        })
        .catch(e => {
          result.innerHTML = '<span style="color: #ef4444;">❌ Ping fehlgeschlagen: ' + e + '</span>';
        });
    }

    function refreshMBusData() {
      // Trigger M-Bus Poll
      fetch('/api/mbus/trigger', { method: 'POST' })
        .then(r => r.json())
        .then(data => {
          console.log('M-Bus Trigger:', data.message);
          // Warte 2 Sekunden und lade dann Stats
          setTimeout(() => {
            fetch('/api/mbus/stats')
              .then(r => r.json())
              .then(stats => {
                const rate = stats.total > 0 ? ((stats.successful / stats.total) * 100).toFixed(1) : 0;
                const avgTime = stats.total > 0 ? (stats.total_time / stats.total).toFixed(0) : 0;
                
                const el = (id) => document.getElementById(id);
                if (el('mbusSuccessRate')) el('mbusSuccessRate').textContent = rate + '%';
                if (el('mbusAvgResponse')) el('mbusAvgResponse').textContent = avgTime + ' ms';
                if (el('mbusLastResponse')) el('mbusLastResponse').textContent = stats.last_response + ' ms';
                if (el('mbusTotalPolls')) el('mbusTotalPolls').textContent = stats.total;
                if (el('mbusHexDump')) el('mbusHexDump').textContent = stats.hex_dump || 'Keine Daten';
              })
              .catch(e => console.error('Stats Fehler:', e));
          }, 2000);
        })
        .catch(e => console.error('Trigger Fehler:', e));
    }

    function resetErrorStats() {
      if (confirm('Möchten Sie die Fehlerstatistik wirklich zurücksetzen?')) {
        fetch('/api/errors/reset', { method: 'POST' })
          .then(r => r.json())
          .then(data => {
            console.log('Fehlerstatistik zurückgesetzt');
            // UI sofort aktualisieren
            document.getElementById('errMbusTimeout').textContent = '0';
            document.getElementById('errMbusParse').textContent = '0';
            document.getElementById('errMqtt').textContent = '0';
            document.getElementById('errWifi').textContent = '0';
            document.getElementById('lastError').style.display = 'none';
            alert('✅ Fehlerstatistik wurde zurückgesetzt!');
          })
          .catch(e => {
            console.error('Reset Fehler:', e);
            alert('❌ Fehler beim Zurücksetzen!');
          });
      }
    }

    function exportData() {
      fetch('/api/data')
        .then(r => r.json())
        .then(data => {
          if (!data.history || data.history.length === 0) {
            alert('Keine Daten zum Exportieren vorhanden!');
            return;
          }
          
          let csv = 'Timestamp,Unix Time,Volume (m),Energy (kWh)\n';
          data.history.forEach(point => {
            const date = new Date(point.timestamp * 1000);
            const dateStr = date.toISOString();
            const energy = (point.volume * data.calorific * data.correction).toFixed(1);
            csv += `${dateStr},${point.timestamp},${point.volume.toFixed(2)},${energy}\n`;
          });
          
          const blob = new Blob([csv], { type: 'text/csv' });
          const url = window.URL.createObjectURL(blob);
          const a = document.createElement('a');
          a.href = url;
          a.download = 'gaszaehler_export_' + new Date().toISOString().split('T')[0] + '.csv';
          a.click();
          window.URL.revokeObjectURL(url);
          
          const result = document.getElementById('diagResult');
          result.style.display = 'block';
          result.textContent = ` CSV Export erfolgreich!\n\n${data.history.length} Datenpunkte exportiert\nDatei: ${a.download}`;
          result.style.color = '#28a745';
        })
        .catch(e => {
          alert('Export fehlgeschlagen: ' + e);
        });
    }

    // Dark Mode initialisieren
    initDarkMode();
    
    updateData();
    updateInterval = setInterval(updateData, 5000);
  </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  String html = String(htmlPage);
  html.replace("%VERSION%", FIRMWARE_VERSION);
  server.send(200, "text/html", html);
}

void handleAPI() {
  String json = "{";
  json += "\"volume\":" + String(lastVolume, 2) + ",";
  json += "\"wifiConnected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  json += "\"wifiRSSI\":" + String(WiFi.RSSI()) + ",";
  json += "\"mqttConnected\":" + String(client.connected() ? "true" : "false") + ",";
  json += "\"apMode\":" + String(apMode ? "true" : "false") + ",";
  json += "\"apSSID\":\"" + String(ap_ssid) + "\",";
  json += "\"ipAddress\":\"" + (apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString()) + "\",";
  json += "\"uptime\":" + String(millis()) + ",";
  json += "\"lastUpdate\":" + String(measurements.empty() ? 0 : measurements.back().timestamp) + ",";
  json += "\"timeInitialized\":" + String(timeInitialized ? "true" : "false") + ",";
  json += "\"pollInterval\":" + String(poll_interval / 1000) + ",";
  // backward-compatible key expected by the WebUI
  json += "\"poll_interval\":" + String(poll_interval / 1000) + ",";
  json += "\"calorific\":" + String(gas_calorific_value, 6) + ",";
  json += "\"correction\":" + String(gas_correction_factor, 6) + ",";
  json += "\"system\":{";
  json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"heapSize\":" + String(ESP.getHeapSize()) + ",";
  json += "\"flashSize\":" + String(ESP.getFlashChipSize()) + ",";
  json += "\"sketchSize\":" + String(ESP.getSketchSize()) + ",";
  json += "\"freeSketch\":" + String(ESP.getFreeSketchSpace()) + ",";
  json += "\"chipModel\":\"" + String(ESP.getChipModel()) + "\",";
  json += "\"chipCores\":" + String(ESP.getChipCores()) + ",";
  json += "\"cpuFreq\":" + String(ESP.getCpuFreqMHz());
  json += "},";
  json += "\"errors\":{";
  json += "\"mbusTimeouts\":" + String(errorStats.mbusTimeouts) + ",";
  json += "\"mbusParseErrors\":" + String(errorStats.mbusParseErrors) + ",";
  json += "\"mqttErrors\":" + String(errorStats.mqttErrors) + ",";
  json += "\"wifiDisconnects\":" + String(errorStats.wifiDisconnects) + ",";
  json += "\"lastError\":\"" + String(errorStats.lastErrorMsg) + "\",";
  json += "\"lastErrorTime\":" + String(errorStats.lastError);
  json += "},";
  json += "\"history\":[";
  for (size_t i = 0; i < measurements.size(); i++) {
    if (i > 0) json += ",";
    json += "{\"timestamp\":" + String(measurements[i].timestamp) + 
            ",\"volume\":" + String(measurements[i].volume, 2) + "}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handleConfigGet() {
  String json = "{";
  json += "\"ssid\":\"" + String(ssid) + "\",";
  json += "\"password\":\"" + String(password) + "\",";
  json += "\"hostname\":\"" + String(hostname) + "\",";
  json += "\"mqtt_server\":\"" + String(mqtt_server) + "\",";
  json += "\"mqtt_port\":" + String(mqtt_port) + ",";
  json += "\"mqtt_user\":\"" + String(mqtt_user) + "\",";
  json += "\"mqtt_pass\":\"" + String(mqtt_pass) + "\",";
  json += "\"mqtt_topic\":\"" + String(mqtt_topic) + "\",";
  json += "\"poll_interval\":" + String(poll_interval / 1000) + ",";
  json += "\"gas_calorific\":" + String(gas_calorific_value, 6) + ",";
  json += "\"gas_correction\":" + String(gas_correction_factor, 6) + ",";
  json += "\"use_static_ip\":" + String(use_static_ip ? "true" : "false") + ",";
  json += "\"static_ip\":\"" + String(static_ip) + "\",";
  json += "\"static_gateway\":\"" + String(static_gateway) + "\",";
  json += "\"static_subnet\":\"" + String(static_subnet) + "\",";
  json += "\"static_dns\":\"" + String(static_dns) + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleLogs() {
  String json = "{";
  json += "\"uptime\":" + String(millis()) + ",";
  json += "\"logs\":[";
  for (size_t i = 0; i < logBuffer.size(); i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"timestamp\":" + String(logBuffer[i].timestamp);
    json += ",\"message\":\"" + logBuffer[i].message + "\"";
    json += "}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handleDiagnostics() {
  String json = "{\"mbus\":{";
  json += "\"total\":" + String(mbusStats.totalPolls) + ",";
  json += "\"successful\":" + String(mbusStats.successfulPolls) + ",";
  json += "\"avgResponseTime\":" + String(mbusStats.avgResponseTime) + ",";
  json += "\"lastResponseTime\":" + String(mbusStats.lastResponseTime);
  json += "}}";
  server.send(200, "application/json", json);
}

void handleWifiScan() {
  Serial.println("WiFi-Scan gestartet...");
  int n = WiFi.scanNetworks();
  
  String json = "{\"networks\":[";
  for (int i = 0; i < n; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"ssid\":\"" + WiFi.SSID(i) + "\"";
    json += ",\"rssi\":" + String(WiFi.RSSI(i));
    json += ",\"encryption\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false");
    json += "}";
  }
  json += "]}";
  
  WiFi.scanDelete();
  server.send(200, "application/json", json);
  Serial.println("WiFi-Scan abgeschlossen: " + String(n) + " Netzwerke gefunden");
}

// Diagnose-Endpunkte
void handleTestMQTT() {
  unsigned long start = millis();
  bool connected = client.connected();
  unsigned long responseTime = millis() - start;
  
  String json = "{";
  json += "\"server\":\"" + String(mqtt_server) + "\",";
  json += "\"port\":" + String(mqtt_port) + ",";
  json += "\"connected\":" + String(connected ? "true" : "false") + ",";
  json += "\"availability_topic\":\"" + String(mqtt_availability_topic) + "\",";
  json += "\"response_time\":" + String(responseTime);
  json += "}";
  server.send(200, "application/json", json);
}

void handleTestWiFi() {
  String json = "{";
  json += "\"ssid\":\"" + WiFi.SSID() + "\",";
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"channel\":" + String(WiFi.channel()) + ",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"mac\":\"" + WiFi.macAddress() + "\",";
  json += "\"hostname\":\"" + String(hostname) + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleTestPing() {
  IPAddress gateway = WiFi.gatewayIP();
  bool reachable = Ping.ping(gateway, 1);
  
  String json = "{";
  json += "\"gateway\":\"" + gateway.toString() + "\",";
  json += "\"reachable\":" + String(reachable ? "true" : "false") + ",";
  json += "\"response_time\":\"" + String(reachable ? "<10ms" : "timeout") + "\",";
  json += "\"dns\":\"" + WiFi.dnsIP().toString() + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleMBusStats() {
  String json = "{";
  json += "\"total\":" + String(mbusStats.totalPolls) + ",";
  json += "\"successful\":" + String(mbusStats.successfulPolls) + ",";
  json += "\"total_time\":" + String(mbusStats.totalResponseTime) + ",";
  json += "\"last_response\":" + String(mbusStats.lastResponseTime) + ",";
  json += "\"hex_dump\":\"" + mbusStats.lastHexDump + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleMBusTrigger() {
  // Manuelle M-Bus Abfrage starten
  if (mbusState == MBUS_IDLE) {
    // Poll Frame senden
    uint8_t pollFrame[5] = {0x10, 0x5B, 0x00, 0x5B, 0x16};
    mbusSerial.write(pollFrame, sizeof(pollFrame));
    mbusSerial.flush();
    mbusLen = 0;
    mbusLastAction = millis();
    mbusState = MBUS_WAIT_RESPONSE;
    
    Serial.println(ANSI_YELLOW "M-Bus: Manuelle Abfrage gestartet" ANSI_RESET);
    server.send(200, "application/json", "{\"status\":\"triggered\",\"message\":\"M-Bus Abfrage gestartet\"}");
  } else {
    server.send(409, "application/json", "{\"status\":\"busy\",\"message\":\"M-Bus Abfrage läuft bereits\"}");
  }
}

void handleErrorReset() {
  // Fehlerstatistik zurücksetzen
  errorStats.mbusTimeouts = 0;
  errorStats.mbusParseErrors = 0;
  errorStats.mqttErrors = 0;
  errorStats.wifiDisconnects = 0;
  lastErrorMessage = "";
  
  Serial.println("Fehlerstatistik zurückgesetzt");
  server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Fehlerstatistik zurückgesetzt\"}");
}


static bool jsonExtractString(const String& body, const char* key, String& out) {
  String pattern = String(""") + key + "":"";
  int idx = body.indexOf(pattern);
  if (idx < 0) return false;
  int start = idx + pattern.length();
  int end = body.indexOf(""", start);
  if (end < 0) return false;
  out = body.substring(start, end);
  return true;
}

static bool jsonExtractNumber(const String& body, const char* key, String& out) {
  String pattern = String(""") + key + "":";
  int idx = body.indexOf(pattern);
  if (idx < 0) return false;
  int start = idx + pattern.length();
  while (start < body.length() && (body.charAt(start) == ' ')) start++;
  int end = start;
  while (end < body.length()) {
    char c = body.charAt(end);
    if ((c >= '0' && c <= '9') || c == '.' || c == '-') end++;
    else break;
  }
  if (end <= start) return false;
  out = body.substring(start, end);
  out.trim();
  return out.length() > 0;
}

static bool jsonExtractBool(const String& body, const char* key, bool& out) {
  String pattern = String(""") + key + "":";
  int idx = body.indexOf(pattern);
  if (idx < 0) return false;
  int start = idx + pattern.length();
  while (start < body.length() && body.charAt(start) == ' ') start++;
  if (body.startsWith("true", start)) { out = true; return true; }
  if (body.startsWith("false", start)) { out = false; return true; }
  return false;
}

void handleConfigPost() {
  Serial.println("DEBUG: handleConfigPost() aufgerufen");
  Serial.println("DEBUG: hasArg('plain') = " + String(server.hasArg("plain") ? "true" : "false"));
  Serial.println("DEBUG: args() = " + String(server.args()));

  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{"error":"invalid request"}");
    return;
  }

  String body = server.arg("plain");
  Serial.println("DEBUG: Body length = " + String(body.length()));
  String tb = body;
  if (tb.length() > 300) tb = tb.substring(0, 300) + "...";
  Serial.println("DEBUG POST /api/config body: " + tb);

  String strVal;
  String numVal;
  bool boolVal;

  if (jsonExtractString(body, "ssid", strVal)) strVal.toCharArray(ssid, sizeof(ssid));
  if (jsonExtractString(body, "password", strVal)) strVal.toCharArray(password, sizeof(password));
  if (jsonExtractString(body, "hostname", strVal)) strVal.toCharArray(hostname, sizeof(hostname));
  if (jsonExtractString(body, "mqtt_server", strVal)) strVal.toCharArray(mqtt_server, sizeof(mqtt_server));
  if (jsonExtractString(body, "mqtt_user", strVal)) strVal.toCharArray(mqtt_user, sizeof(mqtt_user));
  if (jsonExtractString(body, "mqtt_pass", strVal)) strVal.toCharArray(mqtt_pass, sizeof(mqtt_pass));
  if (jsonExtractString(body, "mqtt_topic", strVal)) strVal.toCharArray(mqtt_topic, sizeof(mqtt_topic));
  if (jsonExtractString(body, "static_ip", strVal)) strVal.toCharArray(static_ip, sizeof(static_ip));
  if (jsonExtractString(body, "static_gateway", strVal)) strVal.toCharArray(static_gateway, sizeof(static_gateway));
  if (jsonExtractString(body, "static_subnet", strVal)) strVal.toCharArray(static_subnet, sizeof(static_subnet));
  if (jsonExtractString(body, "static_dns", strVal)) strVal.toCharArray(static_dns, sizeof(static_dns));

  if (jsonExtractNumber(body, "mqtt_port", numVal)) {
    int port = numVal.toInt();
    if (port > 0 && port <= 65535) mqtt_port = port;
  }

  if (jsonExtractNumber(body, "poll_interval", numVal)) {
    int seconds = numVal.toInt();
    if (seconds >= 10 && seconds <= 300) {
      poll_interval = (unsigned long)seconds * 1000UL;
      if (poll_interval < 10000UL) poll_interval = 10000UL;
      if (poll_interval > 300000UL) poll_interval = 300000UL;

      preferences.begin("gas-config", false);
      size_t written = preferences.putULong("poll_interval", poll_interval);
      unsigned long rb = preferences.getULong("poll_interval", 0);
      preferences.end();
      Serial.println("DEBUG: poll_interval in Flash geschrieben: " + String(poll_interval) + " ms (written=" + String(written) + " readback=" + String(rb) + ")");
    } else {
      Serial.println("DEBUG: poll_interval ungültig ('" + numVal + "'), beibehalten: " + String(poll_interval) + " ms");
    }
  }

  if (jsonExtractNumber(body, "gas_calorific", numVal)) {
    float v = numVal.toFloat();
    gas_calorific_value = (v >= 8.0 && v <= 13.0) ? v : 10.0;
  }

  if (jsonExtractNumber(body, "gas_correction", numVal)) {
    float v = numVal.toFloat();
    gas_correction_factor = (v >= 0.90 && v <= 1.10) ? v : 1.0;
  }

  if (jsonExtractBool(body, "use_static_ip", boolVal)) {
    use_static_ip = boolVal;
  }

  saveConfig();
  server.send(200, "application/json", "{"status":"ok"}");

  Serial.println("Konfiguration gespeichert.");
  if (apMode) {
    Serial.println("Wechsel zu Station-Modus in 3 Sekunden...");
  } else {
    Serial.println("Neustart in 3 Sekunden...");
  }
  delay(3000);
  ESP.restart();
}

void setupWebServer() {
  Serial.println("\n=== WebServer Setup Start ===");
  
  // Routen registrieren
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/data", HTTP_GET, handleAPI);
  server.on("/api/config", HTTP_GET, handleConfigGet);
  server.on("/api/config", HTTP_POST, handleConfigPost);
  server.on("/api/wifi/scan", HTTP_GET, handleWifiScan);
  server.on("/api/logs", HTTP_GET, handleLogs);
  server.on("/api/diagnostics", HTTP_GET, handleDiagnostics);
  
  // Diagnose-Endpunkte
  server.on("/api/test/mqtt", HTTP_GET, handleTestMQTT);
  server.on("/api/test/wifi", HTTP_GET, handleTestWiFi);
  server.on("/api/test/ping", HTTP_GET, handleTestPing);
  server.on("/api/mbus/stats", HTTP_GET, handleMBusStats);
  server.on("/api/mbus/trigger", HTTP_POST, handleMBusTrigger);
  server.on("/api/errors/reset", HTTP_POST, handleErrorReset);
  
  // OTA Update über ArduinoOTA (Port 3232) - siehe ArduinoOTA.begin() in setup()
  // WebUI zeigt Anleitung für PlatformIO OTA Upload
  
  // Server starten auf Port 80
  server.begin();
  
  Serial.println("WebServer Routen registriert:");
  Serial.println("  GET  /");
  Serial.println("  GET  /api/data");
  Serial.println("  GET  /api/config");
  Serial.println("  POST /api/config");
  Serial.println("  GET  /api/wifi/scan");
  Serial.println("  GET  /api/logs");
  Serial.println("  ArduinoOTA aktiv (Port 3232)");
  
  String ip = apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  Serial.println(ANSI_GREEN "\n========================================" ANSI_RESET);
  Serial.println(ANSI_GREEN ANSI_BOLD "   WEBSERVER GESTARTET" ANSI_RESET);
  Serial.println(ANSI_GREEN "========================================" ANSI_RESET);
  Serial.println(ANSI_CYAN "Modus: " ANSI_RESET + String(apMode ? "Access Point" : "Station"));
  Serial.println(ANSI_CYAN "IP-Adresse: " ANSI_RESET ANSI_YELLOW ANSI_BOLD + ip + ANSI_RESET);
  Serial.println(ANSI_CYAN "Hostname: " ANSI_RESET + String(hostname));
  Serial.println(ANSI_CYAN "Port: " ANSI_RESET "80");
  Serial.println(ANSI_MAGENTA "\nZugriff:" ANSI_RESET);
  Serial.println(ANSI_YELLOW "  http://" + ip + ANSI_RESET);
  if (!apMode && strlen(hostname) > 0) {
    Serial.println(ANSI_YELLOW "  http://" + String(hostname) + ".local" ANSI_RESET);
  }
  Serial.println(ANSI_GREEN "========================================\n" ANSI_RESET);
}

// ---- Setup ----
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\nESP32 Gaszähler Gateway v" + String(FIRMWARE_VERSION));
  Serial.println("================================");
  
  // Status LED
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);
  addLog("Hardware initialisiert");
  
  // Reset Button konfigurieren
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  
  // Prfen ob BOOT-Button beim Start gedrckt ist (LOW = gedrckt)
  if (digitalRead(RESET_BUTTON_PIN) == LOW) {
    Serial.println(ANSI_RED ANSI_BOLD "\n*** CONFIG RESET ERKANNT ***" ANSI_RESET);
    Serial.println(ANSI_YELLOW "BOOT-Button war beim Start gedrckt." ANSI_RESET);
    Serial.println(ANSI_YELLOW "Lsche gespeicherte Konfiguration..." ANSI_RESET);
    
    preferences.begin("gas-config", false);
    preferences.clear();
    preferences.end();
    
    Serial.println(ANSI_GREEN "Konfiguration gelscht!" ANSI_RESET);
    Serial.println(ANSI_CYAN "Starte im Access Point Modus...\n" ANSI_RESET);
    
    // Defaults setzen
    strcpy(ssid, "SSID");
    strcpy(password, "Password");
    strcpy(hostname, "ESP32-GasZaehler");
    strcpy(mqtt_server, "192.168.178.1");
    mqtt_port = 1883;
    strcpy(mqtt_topic, "gaszaehler/verbrauch");
    poll_interval = 30000;
    
    delay(1000); // Warten damit Button losgelassen werden kann
  }
  
  addLog("Lade Konfiguration...");
  loadConfig();
  addLog("Starte WiFi...");
  setup_wifi();
  
  // Kurze Pause nach WiFi-Setup
  delay(1000);
  
  // mDNS starten (nur im Station-Modus)
  if (WiFi.status() == WL_CONNECTED && !apMode) {
    if (MDNS.begin(hostname)) {
      Serial.println("mDNS gestartet: " + String(hostname) + ".local");
      MDNS.addService("http", "tcp", 80);
    } else {
      Serial.println("mDNS Start fehlgeschlagen");
    }
  }
  
  // NTP Zeit initialisieren
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Synchronisiere Zeit mit NTP...");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      timeInitialized = true;
      Serial.println("Zeit synchronisiert");
    } else {
      Serial.println("Zeit-Synchronisation fehlgeschlagen");
    }
  }
  
  client.setServer(mqtt_server, mqtt_port);
  client.setBufferSize(512); // Grerer Buffer fr Discovery
  
  // Client-ID mit MAC-Adresse fr Eindeutigkeit
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(mqtt_client_id, sizeof(mqtt_client_id), "ESP32Gas-%02X%02X%02X", mac[3], mac[4], mac[5]);
  Serial.println("MQTT Client-ID: " + String(mqtt_client_id));

  setupOTA();
  
  // WebServer starten (funktioniert sowohl im AP als auch Station-Modus)
  setupWebServer();
  
  // M-Bus initialisieren
  mbusSerial.begin(MBUS_BAUD, SERIAL_8E1, MBUS_RX_PIN, MBUS_TX_PIN);
  Serial.println("M-Bus UART bereit");
  
  mbusLastAction = millis() - poll_interval; // sofort Poll starten
  
  addLog("Setup abgeschlossen - System bereit");
  Serial.println(ANSI_GREEN ANSI_BOLD "Setup abgeschlossen!" ANSI_RESET);
  Serial.println(ANSI_CYAN "================================\n" ANSI_RESET);
}

// ---- Loop ----
void loop() {
  // Im AP-Modus nur WebServer und OTA
  if (apMode) {
    ArduinoOTA.handle();
    server.handleClient();
    updateStatusLED();
    return;
  }
  
  // WLAN Check
  if (WiFi.status() != WL_CONNECTED) {
    errorStats.wifiDisconnects++;
    logError("WLAN Verbindung verloren");
    setup_wifi();
  }
  
  if (!client.connected()) reconnect();
  client.loop();
  ArduinoOTA.handle();
  server.handleClient();
  updateStatusLED();
  
  // Memory Check alle 60 Sekunden
  unsigned long now = millis();
  if (now - lastMemoryCheck >= MEMORY_CHECK_INTERVAL) {
    checkMemory();
    lastMemoryCheck = now;
  }
  
  // Status alle 60 Sekunden ausgeben
  static unsigned long lastStatusPrint = 0;
  if (now - lastStatusPrint >= 60000) {
    Serial.println("\n[Status] WiFi: " + String(WiFi.status() == WL_CONNECTED ? "OK" : "FEHLER") + 
                   " | MQTT: " + String(client.connected() ? "OK" : "FEHLER") +
                   " | IP: " + WiFi.localIP().toString() +
                   " | Uptime: " + String(millis()/1000) + "s");
    lastStatusPrint = now;
  }
  
  // Home Assistant Discovery senden (einmalig nach Connect)
  if (client.connected() && !haDiscoverySent) {
    sendHomeAssistantDiscovery();
  }

  switch (mbusState) {
    case MBUS_IDLE:
      if (now - mbusLastAction >= poll_interval) {
        // Poll senden
        uint8_t pollFrame[5] = {0x10, 0x5B, 0x00, 0x5B, 0x16};
        mbusSerial.write(pollFrame, sizeof(pollFrame));
        mbusSerial.flush();
        mbusLen = 0;
        mbusLastAction = now;
        mbusState = MBUS_WAIT_RESPONSE;
        Serial.println("MBUS Poll gesendet, warte auf Antwort...");
        addLog("M-Bus: Poll gestartet");
      }
      break;

    case MBUS_WAIT_RESPONSE:
      while (mbusSerial.available() && mbusLen < sizeof(mbusBuffer)) {
        mbusBuffer[mbusLen++] = mbusSerial.read();
      }

      if ((now - mbusLastAction >= MBUS_RESPONSE_TIMEOUT) || mbusLen >= sizeof(mbusBuffer)) {
        mbusStats.totalPolls++;
        mbusStats.lastResponseTime = now - mbusLastAction;
        mbusStats.totalResponseTime += mbusStats.lastResponseTime;
        
        if (mbusLen > 0) {
          Serial.print("MBUS Antwort empfangen (");
          Serial.print(mbusLen);
          Serial.print(" Bytes, ");
          Serial.print(mbusStats.lastResponseTime);
          Serial.println("ms)");
          addLog("M-Bus: Antwort erhalten (" + String(mbusLen) + " Bytes, " + String(mbusStats.lastResponseTime) + "ms)");
          
          // Hex Dump speichern
          mbusStats.lastHexDump = "";
          String hexLog = "";
          for (size_t i = 0; i < min(mbusLen, (size_t)32); i++) {
            char hex[4];
            sprintf(hex, "%02X ", mbusBuffer[i]);
            mbusStats.lastHexDump += hex;
            hexLog += hex;
          }
          if (mbusLen > 32) hexLog += "...";
          addLog("M-Bus: Rohdaten - " + hexLog);

          float volume = parseGasVolumeBCD(mbusBuffer, mbusLen);
          if (volume >= 0) {
            mbusStats.successfulPolls++;
            
            // Durchschnittliche Antwortzeit berechnen
            mbusStats.avgResponseTime = mbusStats.totalPolls > 0 ? 
              (mbusStats.totalResponseTime / mbusStats.totalPolls) : 0;
            
            char payload[16];
            dtostrf(volume, 0, 2, payload);
            
            // Volumen publishen (retained so Home Assistant always has latest state)
            if (client.publish(mqtt_topic, payload, true)) {
              Serial.print("Verbrauch gesendet: ");
              Serial.println(payload);
              addLog("M-Bus: Verbrauch OK - " + String(payload) + " m³");
              
              // Energie berechnen und publishen (für Energy Dashboard)
              float energy_kwh = volume * gas_calorific_value * gas_correction_factor;
              char energy_payload[16];
              dtostrf(energy_kwh, 0, 1, energy_payload);
              String energy_topic = String(mqtt_topic) + "_energy";
              client.publish(energy_topic.c_str(), energy_payload, true); // retained!
              Serial.print("Energie gesendet: ");
              Serial.print(energy_payload);
              Serial.println(" kWh");
              addLog("MQTT: Energie - " + String(energy_payload) + " kWh (Zählerstand: " + String(payload) + " m³, Brennwert: " + String(gas_calorific_value, 6) + ", Z-Zahl: " + String(gas_correction_factor, 6) + ")");
              
              // Additional HA sensors (nach Energy-Publish)
              String wifiTopic = String(mqtt_topic) + "_wifi";
              client.publish(wifiTopic.c_str(), String(WiFi.RSSI()).c_str(), true); // retained!
              
              String rateTopic = String(mqtt_topic) + "_mbus_rate";
              float rate = mbusStats.totalPolls > 0 ? (mbusStats.successfulPolls * 100.0 / mbusStats.totalPolls) : 0;
              client.publish(rateTopic.c_str(), String(rate, 1).c_str(), true); // retained!
            } else {
              errorStats.mqttErrors++;
              logError("MQTT Publish fehlgeschlagen");
              addLog("MQTT: Publish Fehler");
            }
            
            // Verlauf speichern mit echter Zeit wenn verfgbar
            lastVolume = volume;
            unsigned long timestamp;
            if (timeInitialized) {
              timestamp = time(nullptr);
            } else {
              // Fallback: Unix-Timestamp schätzen basierend auf millis()
              // Verwende einen Basis-Timestamp (01.01.2024) + millis/1000
              timestamp = 1704067200 + (millis() / 1000);
            }
            measurements.push_back({timestamp, volume});
            if (measurements.size() > MAX_MEASUREMENTS) {
              measurements.erase(measurements.begin());
            }
            
            // Alle 10 Messungen persistieren
            static int saveCounter = 0;
            saveCounter++;
            if (saveCounter >= 10) {
              saveHistory();
              saveCounter = 0;
            }
          } else {
            Serial.println("Kein Volumenwert gefunden!");
            errorStats.mbusParseErrors++;
            logError("M-Bus Parse Fehler");
          }
        } else {
          Serial.println("Keine MBUS Antwort erhalten");
          errorStats.mbusTimeouts++;
          logError("M-Bus Timeout");
        }
        mbusState = MBUS_IDLE; // wieder bereit fr nchsten Poll
      }
      break;
  }
}



