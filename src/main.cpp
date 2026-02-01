#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <time.h>
#include <vector>

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
unsigned long poll_interval = 60000; // Standard: 60 Sekunden

Preferences preferences;
bool haDiscoverySent = false;

// ---- WiFi AP Mode ----
bool apMode = false;
const char* ap_ssid = "ESP32-GasZaehler";
const char* ap_password = ""; // Mindestens 8 Zeichen
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

// Automatische Sommerzeit-Erkennung für Europa
bool isDST(time_t now) {
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  int month = timeinfo.tm_mon + 1;
  int day = timeinfo.tm_mday;
  int dow = timeinfo.tm_wday; // 0=Sonntag
  
  // Oktober bis März: Winterzeit
  if (month < 3 || month > 10) return false;
  // April bis September: Sommerzeit
  if (month > 3 && month < 10) return true;
  
  // März: Sommerzeit ab letztem Sonntag 2 Uhr
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
  String lastHexDump = "";
};
MBusStats mbusStats;

// ---- Live Log System ----
const int MAX_LOG_ENTRIES = 50;
struct LogEntry {
  unsigned long timestamp;
  String message;
};
std::vector<LogEntry> logBuffer;

void addLog(const String& msg) {
  LogEntry entry;
  entry.timestamp = millis();
  entry.message = msg;
  logBuffer.push_back(entry);
  
  // Ringbuffer: alte Einträge löschen
  if (logBuffer.size() > MAX_LOG_ENTRIES) {
    logBuffer.erase(logBuffer.begin());
  }
  
  Serial.println("[" + String(millis()/1000) + "s] " + msg);
}

WiFiClient espClient;
PubSubClient client(espClient);
WebServer server(80);

// ---- Verlaufsdaten ----
struct MeasurementData {
  unsigned long timestamp;
  float volume;
};
std::vector<MeasurementData> measurements;
const size_t MAX_MEASUREMENTS = 50;
float lastVolume = -1;

// ---- M-Bus UART ----
HardwareSerial mbusSerial(1); // UART1
const int MBUS_RX_PIN = 16;   // GPIO16 (RX2) für ESP32 DevKit V1
const int MBUS_TX_PIN = 17;   // GPIO17 (TX2) für ESP32 DevKit V1
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
  preferences.begin("gas-config", false);
  
  // Prüfen ob bereits konfiguriert wurde (config_done Flag)
  bool configDone = preferences.getBool("config_done", false);
  
  preferences.getString("ssid", ssid, sizeof(ssid));
  preferences.getString("password", password, sizeof(password));
  preferences.getString("hostname", hostname, sizeof(hostname));
  preferences.getString("mqtt_server", mqtt_server, sizeof(mqtt_server));
  mqtt_port = preferences.getInt("mqtt_port", 1883);
  preferences.getString("mqtt_user", mqtt_user, sizeof(mqtt_user));
  preferences.getString("mqtt_pass", mqtt_pass, sizeof(mqtt_pass));
  preferences.getString("mqtt_topic", mqtt_topic, sizeof(mqtt_topic));
  poll_interval = preferences.getULong("poll_interval", 60000);
  preferences.end();
  
  // Validierung: Poll-Intervall muss zwischen 10s und 1h liegen
  if (poll_interval < 10000) poll_interval = 10000; // Minimum 10s
  if (poll_interval > 3600000) poll_interval = 3600000; // Maximum 1h
  
  // Wenn noch nie konfiguriert oder SSID leer -> Defaults setzen
  if (!configDone || strlen(ssid) == 0) {
    Serial.println("Keine gültige Konfiguration gefunden - verwende Defaults");
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
  if (poll_interval < 10000) poll_interval = 10000;
  if (poll_interval > 3600000) poll_interval = 3600000;
  
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
  preferences.putBool("config_done", true); // Markiere als konfiguriert
  preferences.end();
  
  Serial.println("Konfiguration gespeichert");
  Serial.println("Poll-Intervall: " + String(poll_interval / 1000) + "s (" + String(poll_interval) + "ms)");
}

// ---- Fehler loggen ----
void logError(const char* msg) {
  errorStats.lastError = millis();
  strncpy(errorStats.lastErrorMsg, msg, sizeof(errorStats.lastErrorMsg) - 1);
  errorStats.lastErrorMsg[sizeof(errorStats.lastErrorMsg) - 1] = '\0';
  Serial.print("ERROR: ");
  Serial.println(msg);
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
    apMode = false;
  } else {
    Serial.println("WLAN-Verbindung fehlgeschlagen!");
    logError("WLAN Verbindung fehlgeschlagen");
    Serial.println("Starte Access Point für Konfiguration...");
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
  Serial.print("Passwort: ");
  Serial.println(ap_password);
  Serial.print("IP-Adresse: ");
  Serial.println(IP);
  Serial.println("\nVerbinden Sie sich mit dem Access Point");
  Serial.println("und öffnen Sie http://" + IP.toString());
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
    
    // Last Will Testament für automatische Offline-Erkennung
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
  
  String baseDevice = "{\"identifiers\":[\"esp32_gas_meter\"],\"name\":\"ESP32 Gaszähler\",\"model\":\"BK-G4 M-Bus Gateway\",\"manufacturer\":\"ESP32\",\"sw_version\":\"1.2\",\"configuration_url\":\"http://" + WiFi.localIP().toString() + "\"}";
  
  // 1. Gas Volume Sensor (Main) - Energy Dashboard Ready
  String topic1 = "homeassistant/sensor/gaszaehler_volume/config";
  String payload1 = "{\"name\":\"Gasverbrauch\",\"state_topic\":\"" + String(mqtt_topic) + "\",\"availability_topic\":\"" + String(mqtt_availability_topic) + "\",\"unit_of_measurement\":\"m³\",\"device_class\":\"gas\",\"state_class\":\"total_increasing\",\"last_reset_value_template\":\"1970-01-01T00:00:00+00:00\",\"value_template\":\"{{ value | float }}\",\"suggested_display_precision\":2,\"unique_id\":\"esp32_gas_volume\",\"device\":" + baseDevice + "}";
  client.publish(topic1.c_str(), payload1.c_str(), true);
  
  // 2. WiFi Signal Sensor
  String topic2 = "homeassistant/sensor/gaszaehler_wifi/config";
  String payload2 = "{\"name\":\"Gaszähler WiFi Signal\",\"state_topic\":\"" + String(mqtt_topic) + "_wifi\",\"availability_topic\":\"" + String(mqtt_availability_topic) + "\",\"unit_of_measurement\":\"dBm\",\"device_class\":\"signal_strength\",\"value_template\":\"{{ value }}\",\"unique_id\":\"esp32_gas_wifi\",\"device\":" + baseDevice + "}";
  client.publish(topic2.c_str(), payload2.c_str(), true);
  
  // 3. M-Bus Success Rate Sensor
  String topic3 = "homeassistant/sensor/gaszaehler_mbus_rate/config";
  String payload3 = "{\"name\":\"Gaszähler M-Bus Success Rate\",\"state_topic\":\"" + String(mqtt_topic) + "_mbus_rate\",\"availability_topic\":\"" + String(mqtt_availability_topic) + "\",\"unit_of_measurement\":\"%\",\"value_template\":\"{{ value }}\",\"unique_id\":\"esp32_gas_mbus_rate\",\"icon\":\"mdi:check-network\",\"device\":" + baseDevice + "}";
  client.publish(topic3.c_str(), payload3.c_str(), true);
  
  // 4. Binary Sensor for Connectivity
  String topic4 = "homeassistant/binary_sensor/gaszaehler_connected/config";
  String payload4 = "{\"name\":\"Gaszähler Online\",\"state_topic\":\"" + String(mqtt_availability_topic) + "\",\"payload_on\":\"online\",\"payload_off\":\"offline\",\"device_class\":\"connectivity\",\"unique_id\":\"esp32_gas_connected\",\"device\":" + baseDevice + "}";
  client.publish(topic4.c_str(), payload4.c_str(), true);
  
  Serial.println("Home Assistant Discovery gesendet (4 Entities)");
  haDiscoverySent = true;
}

// ---- BCD Parser für Gaszähler ----
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
            return value / 1000.0; // 2 Dezimalstellen → m³
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
  <title>Gaszähler Monitor</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      padding: 20px;
    }
    .container {
      max-width: 1000px;
      margin: 0 auto;
    }
    .header {
      text-align: center;
      color: white;
      margin-bottom: 30px;
    }
    .header h1 {
      font-size: 2.5em;
      margin-bottom: 10px;
      text-shadow: 2px 2px 4px rgba(0,0,0,0.2);
    }
    .nav {
      display: flex;
      gap: 10px;
      justify-content: center;
      margin-bottom: 20px;
    }
    .nav button {
      background: white;
      border: none;
      padding: 10px 20px;
      border-radius: 8px;
      cursor: pointer;
      font-size: 1em;
      transition: transform 0.2s;
    }
    .nav button:hover {
      transform: translateY(-2px);
    }
    .nav button.active {
      background: #667eea;
      color: white;
    }
    .card {
      background: white;
      border-radius: 15px;
      padding: 25px;
      margin-bottom: 20px;
      box-shadow: 0 10px 30px rgba(0,0,0,0.2);
    }
    .value-display {
      text-align: center;
      padding: 30px;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      border-radius: 10px;
      color: white;
    }
    .value-display .label {
      font-size: 1.2em;
      opacity: 0.9;
      margin-bottom: 10px;
    }
    .value-display .value {
      font-size: 3em;
      font-weight: bold;
      text-shadow: 2px 2px 4px rgba(0,0,0,0.2);
    }
    .status-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
      gap: 15px;
      margin-top: 20px;
    }
    .status-item {
      padding: 15px;
      background: #f8f9fa;
      border-radius: 8px;
      border-left: 4px solid #667eea;
    }
    .status-item .label {
      font-size: 0.9em;
      color: #666;
      margin-bottom: 5px;
    }
    .status-item .value {
      font-size: 1.2em;
      font-weight: bold;
      color: #333;
    }
    .status-online { border-left-color: #28a745; }
    .status-offline { border-left-color: #dc3545; }
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
      margin-bottom: 20px;
    }
    .form-group label {
      display: block;
      margin-bottom: 5px;
      font-weight: bold;
      color: #333;
    }
    .form-group input {
      width: 100%;
      padding: 10px;
      border: 2px solid #ddd;
      border-radius: 8px;
      font-size: 1em;
      transition: border-color 0.3s;
    }
    .form-group input:focus {
      outline: none;
      border-color: #667eea;
    }
    .btn {
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      border: none;
      padding: 12px 30px;
      border-radius: 8px;
      font-size: 1em;
      cursor: pointer;
      transition: transform 0.2s;
    }
    .btn:hover {
      transform: translateY(-2px);
    }
    .page {
      display: none;
    }
    .page.active {
      display: block;
    }
    .alert {
      padding: 15px;
      border-radius: 8px;
      margin-bottom: 20px;
      display: none;
    }
    .alert.success {
      background: #d4edda;
      color: #155724;
      border: 1px solid #c3e6cb;
    }
    .alert.error {
      background: #f8d7da;
      color: #721c24;
      border: 1px solid #f5c6cb;
    }
    @media (max-width: 600px) {
      .header h1 { font-size: 1.8em; }
      .value-display .value { font-size: 2em; }
      .nav { flex-wrap: wrap; }
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>⚡ Gaszähler Monitor</h1>
      <p>ESP32 M-Bus Gateway</p>
      <div id="apModeWarning" style="display: none; background: #ff9800; color: white; padding: 10px; border-radius: 8px; margin-top: 10px;">
        ⚠️ <strong>Access Point Modus aktiv!</strong><br>
        Bitte konfigurieren Sie WLAN unter "Konfiguration" und speichern Sie die Einstellungen.
      </div>
    </div>

    <div class="nav">
      <button onclick="showPage('dashboard')" class="active" id="navDashboard">Dashboard</button>
      <button onclick="showPage('config')" id="navConfig">Konfiguration</button>
      <button onclick="showPage('logs')" id="navLogs">Live Logs</button>
    </div>

    <div id="dashboard" class="page active">
      <div class="card">
        <div class="value-display">
          <div class="label">Aktueller Verbrauch</div>
          <div class="value" id="gasValue">-- m³</div>
        </div>
      </div>

      <div class="card">
        <h2>System Status</h2>
        <div class="status-grid">
          <div class="status-item" id="wifiStatus">
            <div class="label">WLAN</div>
            <div class="value">--</div>
          </div>
          <div class="status-item" id="mqttStatus">
            <div class="label">MQTT</div>
            <div class="value">--</div>
          </div>
          <div class="status-item">
            <div class="label">Uptime</div>
            <div class="value" id="uptime">--</div>
          </div>
          <div class="status-item">
            <div class="label">Letzte Messung</div>
            <div class="value" id="lastUpdate">--</div>
          </div>
          <div class="status-item">
            <div class="label">Poll-Intervall</div>
            <div class="value" id="pollInterval">--</div>
          </div>
          <div class="status-item">
            <div class="label">Status-LED</div>
            <div class="value">GPIO2</div>
          </div>
          <div class="status-item" id="wifiSignal">
            <div class="label">WLAN Signal</div>
            <div class="value">--</div>
          </div>
        </div>
      </div>

      <div class="card">
        <h2>System-Informationen</h2>
        <div class="status-grid">
          <div class="status-item">
            <div class="label">Freier Heap</div>
            <div class="value" id="freeHeap">--</div>
          </div>
          <div class="status-item">
            <div class="label">Heap-Größe</div>
            <div class="value" id="heapSize">--</div>
          </div>
          <div class="status-item">
            <div class="label">Flash-Größe</div>
            <div class="value" id="flashSize">--</div>
          </div>
          <div class="status-item">
            <div class="label">Sketch-Größe</div>
            <div class="value" id="sketchSize">--</div>
          </div>
          <div class="status-item">
            <div class="label">Freier Flash</div>
            <div class="value" id="freeFlash">--</div>
          </div>
          <div class="status-item">
            <div class="label">Chip-Modell</div>
            <div class="value" id="chipModel">--</div>
          </div>
        </div>
      </div>

      <div class="card">
        <h2>Fehlerstatistik</h2>
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
        <div id="lastError" style="margin-top: 15px; padding: 10px; background: #f8f9fa; border-radius: 5px; display: none;">
          <strong>Letzter Fehler:</strong> <span id="lastErrorMsg">--</span>
        </div>
      </div>

      <div class="card">
        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 15px;">
          <h2 style="margin: 0;">Verlauf</h2>
          <div style="display: flex; gap: 5px;">
            <button onclick="setTimeRange(24)" class="btn" id="btn24h" style="padding: 5px 12px; font-size: 0.9em; background: #667eea;">24h</button>
            <button onclick="setTimeRange(168)" class="btn" id="btn7d" style="padding: 5px 12px; font-size: 0.9em; background: white; color: #667eea;">7d</button>
            <button onclick="setTimeRange(720)" class="btn" id="btn30d" style="padding: 5px 12px; font-size: 0.9em; background: white; color: #667eea;">30d</button>
            <button onclick="setTimeRange(0)" class="btn" id="btnAll" style="padding: 5px 12px; font-size: 0.9em; background: white; color: #667eea;">Alle</button>
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
        <h2>Einstellungen</h2>
        <div id="configAlert" class="alert"></div>
        <form id="configForm" onsubmit="saveConfig(event)">
          <h3>WLAN</h3>
          <div class="form-group">
            <label>SSID</label>
            <div style="display: flex; gap: 10px; align-items: start;">
              <input type="text" id="ssid" name="ssid" required style="flex: 1;">
              <button type="button" onclick="scanWifi()" class="btn" style="padding: 10px 20px; white-space: nowrap;">Scannen</button>
            </div>
            <select id="wifiList" onchange="selectWifi()" style="width: 100%; padding: 10px; border: 2px solid #ddd; border-radius: 8px; margin-top: 10px; display: none;">
              <option value="">-- Netzwerk auswählen --</option>
            </select>
          </div>
          <div class="form-group">
            <label>Passwort</label>
            <input type="password" id="password" name="password">
          </div>
          <div class="form-group">
            <label>Hostname</label>
            <input type="text" id="hostname" name="hostname" required>
            <small style="color: #666;">Für mDNS (z.B. ESP32-GasZaehler.local)</small>
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
            <small style="color: #666;">Leer lassen wenn keine Authentifizierung</small>
          </div>
          <div class="form-group">
            <label>Passwort (optional)</label>
            <input type="password" id="mqtt_pass" name="mqtt_pass">
          </div>
          <div class="form-group">
            <label>Topic</label>
            <input type="text" id="mqtt_topic" name="mqtt_topic" required>
          </div>
          
          <h3 style="margin-top: 30px;">Abfrage-Einstellungen</h3>
          <div class="form-group">
            <label>Poll-Intervall (Sekunden)</label>
            <input type="number" id="poll_interval" name="poll_interval" min="10" max="3600" required>
            <small style="color: #666;">Wie oft der Gaszähler abgefragt wird (10-3600 Sekunden)</small>
          </div>
          
          <button type="submit" class="btn">Speichern & Neustart</button>
        </form>
      </div>
    </div>

    <div id="logs" class="page">
      <div class="card">
        <h2>Live Logs</h2>
        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 15px;">
          <span style="color: #666;">Zeigt die letzten 50 Log-Einträge</span>
          <button onclick="refreshLogs()" class="btn" style="padding: 8px 16px;">Aktualisieren</button>
        </div>
        <div id="logContainer" style="background: #f8f9fa; border-radius: 8px; padding: 15px; max-height: 600px; overflow-y: auto; font-family: monospace; font-size: 0.9em;">
          <div style="text-align: center; color: #999;">Lade Logs...</div>
        </div>
      </div>
    </div>
  </div>

  <script>
    let currentPage = 'dashboard';
    let updateInterval = null;
    let timeRangeHours = 24;
    let fullHistoryData = [];
    let chartZoom = 1.0;
    let chartOffsetX = 0;
    let lastChartData = [];

    function showPage(page) {
      document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
      document.querySelectorAll('.nav button').forEach(b => b.classList.remove('active'));
      document.getElementById(page).classList.add('active');
      document.getElementById('nav' + page.charAt(0).toUpperCase() + page.slice(1)).classList.add('active');
      currentPage = page;
      
      if (page === 'dashboard') {
        updateInterval = setInterval(updateData, 5000);
        updateData();
      } else {
        if (updateInterval) clearInterval(updateInterval);
        if (page === 'config') loadConfig();
        if (page === 'logs') {
          refreshLogs();
          updateInterval = setInterval(refreshLogs, 3000);
        }
      }
    }

    function updateData() {
      fetch('/api/data')
        .then(r => r.json())
        .then(data => {
          // AP-Modus Warnung anzeigen
          if (data.apMode) {
            document.getElementById('apModeWarning').style.display = 'block';
          }
          
          document.getElementById('gasValue').textContent = 
            data.volume >= 0 ? data.volume.toFixed(2) + ' m³' : '-- m³';
          
          const wifiDiv = document.getElementById('wifiStatus');
          if (data.apMode) {
            wifiDiv.querySelector('.value').textContent = 'AP-Modus (' + data.apSSID + ')';
            wifiDiv.className = 'status-item';
            wifiDiv.style.borderLeftColor = '#ff9800';
          } else {
            wifiDiv.querySelector('.value').textContent = data.wifiConnected ? 'Verbunden' : 'Getrennt';
            wifiDiv.className = data.wifiConnected ? 'status-item status-online' : 'status-item status-offline';
          }
          
          // WiFi Signal Strength
          const signalDiv = document.getElementById('wifiSignal');
          if (data.wifiConnected && !data.apMode) {
            const rssi = data.wifiRSSI;
            let quality = 'Schlecht';
            let color = '#dc3545';
            let bars = '▂░░░';
            
            if (rssi >= -50) {
              quality = 'Ausgezeichnet';
              color = '#28a745';
              bars = '▂▄▆█';
            } else if (rssi >= -60) {
              quality = 'Gut';
              color = '#28a745';
              bars = '▂▄▆░';
            } else if (rssi >= -70) {
              quality = 'Mittel';
              color = '#ffc107';
              bars = '▂▄░░';
            } else if (rssi >= -80) {
              quality = 'Schwach';
              color = '#ff9800';
              bars = '▂░░░';
            }
            
            signalDiv.querySelector('.value').textContent = bars + ' ' + rssi + ' dBm (' + quality + ')';
            signalDiv.style.borderLeftColor = color;
          } else {
            signalDiv.querySelector('.value').textContent = '--';
            signalDiv.style.borderLeftColor = '#667eea';
          }
          
          const mqttDiv = document.getElementById('mqttStatus');
          mqttDiv.querySelector('.value').textContent = data.mqttConnected ? 'Verbunden' : 'Getrennt';
          mqttDiv.className = data.mqttConnected ? 'status-item status-online' : 'status-item status-offline';
          
          document.getElementById('uptime').textContent = formatUptime(data.uptime);
          
          // Zeitstempel-Anzeige (NTP oder relative Zeit)
          if (data.timeInitialized && data.lastUpdate > 1000000000) {
            const date = new Date(data.lastUpdate * 1000);
            document.getElementById('lastUpdate').textContent = date.toLocaleTimeString('de-DE');
          } else {
            document.getElementById('lastUpdate').textContent = 
              data.lastUpdate > 0 ? Math.floor((millis() - data.lastUpdate) / 1000) + 's' : '--';
          }
          
          document.getElementById('pollInterval').textContent = data.pollInterval + 's';
          
          // System Info
          if (data.system) {
            document.getElementById('freeHeap').textContent = (data.system.freeHeap / 1024).toFixed(1) + ' KB';
            document.getElementById('heapSize').textContent = (data.system.heapSize / 1024).toFixed(1) + ' KB';
            document.getElementById('flashSize').textContent = (data.system.flashSize / 1024 / 1024).toFixed(1) + ' MB';
            document.getElementById('sketchSize').textContent = (data.system.sketchSize / 1024).toFixed(1) + ' KB';
            document.getElementById('freeFlash').textContent = (data.system.freeSketch / 1024).toFixed(1) + ' KB';
            document.getElementById('chipModel').textContent = data.system.chipModel + ' (' + data.system.chipCores + ' Cores @ ' + data.system.cpuFreq + 'MHz)';
          }
          
          // Error Stats
          document.getElementById('errMbusTimeout').textContent = data.errors.mbusTimeouts;
          document.getElementById('errMbusParse').textContent = data.errors.mbusParseErrors;
          document.getElementById('errMqtt').textContent = data.errors.mqttErrors;
          document.getElementById('errWifi').textContent = data.errors.wifiDisconnects;
          
          // Letzter Fehler nur anzeigen wenn:
          // 1. Es einen Fehler gibt UND
          // 2. Der Fehler nicht älter als 5 Minuten ist UND
          // 3. Es tatsächlich Fehler gibt (Counter > 0)
          const totalErrors = data.errors.mbusTimeouts + data.errors.mbusParseErrors + 
                             data.errors.mqttErrors + data.errors.wifiDisconnects;
          const errorAge = data.uptime - data.errors.lastErrorTime;
          const fiveMinutes = 5 * 60 * 1000;
          
          if (data.errors.lastError && totalErrors > 0 && errorAge < fiveMinutes) {
            document.getElementById('lastError').style.display = 'block';
            const ageText = errorAge < 60000 ? 
              'vor ' + Math.floor(errorAge / 1000) + 's' : 
              'vor ' + Math.floor(errorAge / 60000) + 'min';
            document.getElementById('lastErrorMsg').textContent = data.errors.lastError + ' (' + ageText + ')';
          } else {
            document.getElementById('lastError').style.display = 'none';
          }
          
          if (data.history && data.history.length > 0) {
            fullHistoryData = data.history;
            drawChart(filterHistoryByTimeRange(data.history));
          }
        })
        .catch(e => console.error('Fehler:', e));
    }

    function loadConfig() {
      fetch('/api/config')
        .then(r => r.json())
        .then(data => {
          document.getElementById('ssid').value = data.ssid;
          document.getElementById('password').value = data.password;
          document.getElementById('hostname').value = data.hostname || 'ESP32-GasZaehler';
          document.getElementById('mqtt_server').value = data.mqtt_server;
          document.getElementById('mqtt_port').value = data.mqtt_port;
          document.getElementById('mqtt_user').value = data.mqtt_user || '';
          document.getElementById('mqtt_pass').value = data.mqtt_pass || '';
          document.getElementById('mqtt_topic').value = data.mqtt_topic;
          document.getElementById('poll_interval').value = data.poll_interval;
        })
        .catch(e => console.error('Fehler:', e));
    }

    function refreshLogs() {
      fetch('/api/logs')
        .then(r => r.json())
        .then(data => {
          const container = document.getElementById('logContainer');
          if (data.logs.length === 0) {
            container.innerHTML = '<div style="text-align: center; color: #999;">Keine Logs verfügbar</div>';
            return;
          }
          
          let html = '';
          // Neueste zuerst
          for (let i = data.logs.length - 1; i >= 0; i--) {
            const log = data.logs[i];
            const time = Math.floor(log.timestamp / 1000);
            const color = log.message.includes('Fehler') || log.message.includes('ERROR') ? '#dc3545' : 
                         log.message.includes('Verbunden') || log.message.includes('OK') ? '#28a745' : '#333';
            html += `<div style="margin-bottom: 5px; color: ${color};">`;
            html += `<span style="color: #666;">[${time}s]</span> ${log.message}`;
            html += '</div>';
          }
          container.innerHTML = html;
          
          // Auto-scroll nach unten wenn nötig
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
          select.innerHTML = '<option value="">-- Netzwerk auswählen --</option>';
          
          data.networks.forEach(net => {
            const option = document.createElement('option');
            const signal = net.rssi > -50 ? '███' : net.rssi > -70 ? '██░' : '█░░';
            const lock = net.encryption ? '🔒' : '🔓';
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
        poll_interval: parseInt(formData.get('poll_interval'))
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

    function setTimeRange(hours) {
      timeRangeHours = hours;
      
      // Update button styles
      document.querySelectorAll('[id^="btn"]').forEach(btn => {
        btn.style.background = 'white';
        btn.style.color = '#667eea';
      });
      
      if (hours === 24) document.getElementById('btn24h').style.background = '#667eea';
      else if (hours === 168) document.getElementById('btn7d').style.background = '#667eea';
      else if (hours === 720) document.getElementById('btn30d').style.background = '#667eea';
      else document.getElementById('btnAll').style.background = '#667eea';
      
      document.querySelectorAll('[id^="btn"]').forEach(btn => {
        if (btn.style.background === 'rgb(102, 126, 234)' || btn.style.background === '#667eea') {
          btn.style.color = 'white';
        }
      });
      
      drawChart(filterHistoryByTimeRange(fullHistoryData));
    }
    
    function filterHistoryByTimeRange(history) {
      if (!history || history.length === 0) return history;
      if (timeRangeHours === 0) return history; // Alle anzeigen
      
      const now = Date.now() / 1000;
      const cutoff = now - (timeRangeHours * 3600);
      
      return history.filter(point => point.timestamp >= cutoff);
    }

    function setTimeRange(hours) {
      timeRangeHours = hours;
      
      // Update button styles
      document.querySelectorAll('[id^="btn"]').forEach(btn => {
        btn.style.background = 'white';
        btn.style.color = '#667eea';
      });
      
      if (hours === 24) document.getElementById('btn24h').style.background = '#667eea';
      else if (hours === 168) document.getElementById('btn7d').style.background = '#667eea';
      else if (hours === 720) document.getElementById('btn30d').style.background = '#667eea';
      else document.getElementById('btnAll').style.background = '#667eea';
      
      document.querySelectorAll('[id^="btn"]').forEach(btn => {
        if (btn.style.background === 'rgb(102, 126, 234)' || btn.style.background === '#667eea') {
          btn.style.color = 'white';
        }
      });
      
      drawChart(filterHistoryByTimeRange(fullHistoryData));
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

    function drawChart(history) {
      const canvas = document.getElementById('chart');
      const ctx = canvas.getContext('2d');
      canvas.width = canvas.offsetWidth;
      canvas.height = canvas.offsetHeight;
      
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      
      if (!history || history.length === 0) {
        ctx.fillStyle = '#999';
        ctx.font = '16px sans-serif';
        ctx.textAlign = 'center';
        ctx.fillText('Keine Daten für diesen Zeitraum', canvas.width / 2, canvas.height / 2);
        lastChartData = [];
        return;
      }
      
      if (history.length < 2) return;
      
      const values = history.map(h => h.volume);
      const min = Math.min(...values) * 0.95;
      const max = Math.max(...values) * 1.05;
      const range = max - min || 1;
      
      const padding = 40;
      const chartWidth = canvas.width - padding * 2;
      const chartHeight = canvas.height - padding * 2;
      
      // Achsen
      ctx.strokeStyle = '#ddd';
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(padding, padding);
      ctx.lineTo(padding, canvas.height - padding);
      ctx.lineTo(canvas.width - padding, canvas.height - padding);
      ctx.stroke();
      
      // Grid lines
      ctx.strokeStyle = '#f0f0f0';
      ctx.lineWidth = 0.5;
      for (let i = 1; i < 5; i++) {
        const y = padding + (chartHeight / 5) * i;
        ctx.beginPath();
        ctx.moveTo(padding, y);
        ctx.lineTo(canvas.width - padding, y);
        ctx.stroke();
      }
      
      // Linie mit Gradient
      const gradient = ctx.createLinearGradient(0, padding, 0, canvas.height - padding);
      gradient.addColorStop(0, '#667eea');
      gradient.addColorStop(1, '#764ba2');
      ctx.strokeStyle = gradient;
      ctx.lineWidth = 3;
      ctx.beginPath();
      
      lastChartData = [];
      history.forEach((point, i) => {
        const x = padding + (i / (history.length - 1)) * chartWidth;
        const y = canvas.height - padding - ((point.volume - min) / range) * chartHeight;
        lastChartData.push({x, y, volume: point.volume, timestamp: point.timestamp});
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      });
      ctx.stroke();
      
      // Punkte
      ctx.fillStyle = '#667eea';
      lastChartData.forEach(point => {
        ctx.beginPath();
        ctx.arc(point.x, point.y, 5, 0, 2 * Math.PI);
        ctx.fill();
        ctx.strokeStyle = 'white';
        ctx.lineWidth = 2;
        ctx.stroke();
      });
      
      // Beschriftung
      ctx.fillStyle = '#666';
      ctx.font = '12px sans-serif';
      ctx.fillText(max.toFixed(2) + ' m³', 5, padding);
      ctx.fillText(min.toFixed(2) + ' m³', 5, canvas.height - padding + 5);
    }
    
    // Chart Mouse Interaction for Tooltips
    document.addEventListener('DOMContentLoaded', function() {
      const canvas = document.getElementById('chart');
      const tooltip = document.getElementById('chartTooltip');
      
      canvas.addEventListener('mousemove', function(e) {
        if (lastChartData.length === 0) return;
        
        const rect = canvas.getBoundingClientRect();
        const mouseX = e.clientX - rect.left;
        const mouseY = e.clientY - rect.top;
        
        let closestPoint = null;
        let minDist = Infinity;
        
        lastChartData.forEach(point => {
          const dist = Math.sqrt(Math.pow(mouseX - point.x, 2) + Math.pow(mouseY - point.y, 2));
          if (dist < minDist && dist < 20) {
            minDist = dist;
            closestPoint = point;
          }
        });
        
        if (closestPoint) {
          // Prüfen ob Unix-Timestamp oder millis()
          const isUnixTime = closestPoint.timestamp > 1000000000;
          let timeStr;
          
          if (isUnixTime) {
            const date = new Date(closestPoint.timestamp * 1000);
            timeStr = date.toLocaleString('de-DE', {hour: '2-digit', minute: '2-digit', day: '2-digit', month: '2-digit'});
          } else {
            // Bei millis() relative Zeit anzeigen
            const seconds = Math.floor(closestPoint.timestamp / 1000);
            const minutes = Math.floor(seconds / 60);
            const hours = Math.floor(minutes / 60);
            if (hours > 0) timeStr = `vor ${hours}h ${minutes % 60}m`;
            else if (minutes > 0) timeStr = `vor ${minutes}m`;
            else timeStr = `vor ${seconds}s`;
          }
          
          tooltip.innerHTML = `<strong>${closestPoint.volume.toFixed(2)} m³</strong><br>${timeStr}`;
          tooltip.style.display = 'block';
          tooltip.style.left = (e.clientX + 15) + 'px';
          tooltip.style.top = (e.clientY - 40) + 'px';
          canvas.style.cursor = 'pointer';
        } else {
          tooltip.style.display = 'none';
          canvas.style.cursor = 'crosshair';
        }
      });
      
      canvas.addEventListener('mouseleave', function() {
        tooltip.style.display = 'none';
      });
      
      // Zoom with Mouse Wheel
      canvas.addEventListener('wheel', function(e) {
        e.preventDefault();
        const delta = e.deltaY > 0 ? 0.9 : 1.1;
        chartZoom *= delta;
        chartZoom = Math.max(0.5, Math.min(3, chartZoom));
        drawChart(filterHistoryByTimeRange(fullHistoryData));
      });
    });

    updateData();
    updateInterval = setInterval(updateData, 5000);
  </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", htmlPage);
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
  json += "\"poll_interval\":" + String(poll_interval / 1000);
  json += "}";
  server.send(200, "application/json", json);
}

void handleLogs() {
  String json = "{\"logs\":[";
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

void handleConfigPost() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    
    // Einfaches JSON Parsing (für kleine Daten ausreichend)
    int idx;
    
    idx = body.indexOf("\"ssid\":\"");
    if (idx >= 0) {
      int start = idx + 8;
      int end = body.indexOf("\"", start);
      String val = body.substring(start, end);
      val.toCharArray(ssid, sizeof(ssid));
    }
    
    idx = body.indexOf("\"password\":\"");
    if (idx >= 0) {
      int start = idx + 12;
      int end = body.indexOf("\"", start);
      String val = body.substring(start, end);
      val.toCharArray(password, sizeof(password));
    }
    
    idx = body.indexOf("\"hostname\":\"");
    if (idx >= 0) {
      int start = idx + 12;
      int end = body.indexOf("\"", start);
      String val = body.substring(start, end);
      val.toCharArray(hostname, sizeof(hostname));
    }
    
    idx = body.indexOf("\"mqtt_server\":\"");
    if (idx >= 0) {
      int start = idx + 15;
      int end = body.indexOf("\"", start);
      String val = body.substring(start, end);
      val.toCharArray(mqtt_server, sizeof(mqtt_server));
    }
    
    idx = body.indexOf("\"mqtt_port\":");
    if (idx >= 0) {
      int start = idx + 12;
      int end = body.indexOf(",", start);
      if (end < 0) end = body.indexOf("}", start);
      mqtt_port = body.substring(start, end).toInt();
    }
    
    idx = body.indexOf("\"mqtt_user\":\"");
    if (idx >= 0) {
      int start = idx + 13;
      int end = body.indexOf("\"", start);
      String val = body.substring(start, end);
      val.toCharArray(mqtt_user, sizeof(mqtt_user));
    }
    
    idx = body.indexOf("\"mqtt_pass\":\"");
    if (idx >= 0) {
      int start = idx + 13;
      int end = body.indexOf("\"", start);
      String val = body.substring(start, end);
      val.toCharArray(mqtt_pass, sizeof(mqtt_pass));
    }
    
    idx = body.indexOf("\"mqtt_topic\":\"");
    if (idx >= 0) {
      int start = idx + 14;
      int end = body.indexOf("\"", start);
      String val = body.substring(start, end);
      val.toCharArray(mqtt_topic, sizeof(mqtt_topic));
    }
    
    idx = body.indexOf("\"poll_interval\":");
    if (idx >= 0) {
      int start = idx + 17;
      int end = body.indexOf(",", start);
      if (end < 0) end = body.indexOf("}", start);
      poll_interval = body.substring(start, end).toInt() * 1000; // Sekunden -> ms
      if (poll_interval < 10000) poll_interval = 60000; // Minimum 10s
    }
    
    saveConfig();
    server.send(200, "application/json", "{\"status\":\"ok\"}");
    
    Serial.println("Konfiguration gespeichert.");
    if (apMode) {
      Serial.println("Wechsel zu Station-Modus in 3 Sekunden...");
    } else {
      Serial.println("Neustart in 3 Sekunden...");
    }
    delay(3000);
    ESP.restart();
  } else {
    server.send(400, "application/json", "{\"error\":\"invalid request\"}");
  }
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
  
  // Server starten auf Port 80
  server.begin();
  
  Serial.println("WebServer Routen registriert:");
  Serial.println("  GET  /");
  Serial.println("  GET  /api/data");
  Serial.println("  GET  /api/config");
  Serial.println("  POST /api/config");
  Serial.println("  GET  /api/wifi/scan");
  
  String ip = apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  Serial.println("\n========================================");
  Serial.println("   WEBSERVER GESTARTET");
  Serial.println("========================================");
  Serial.println("Modus: " + String(apMode ? "Access Point" : "Station"));
  Serial.println("IP-Adresse: " + ip);
  Serial.println("Hostname: " + String(hostname));
  Serial.println("Port: 80");
  Serial.println("\nZugriff:");
  Serial.println("  http://" + ip);
  if (!apMode && strlen(hostname) > 0) {
    Serial.println("  http://" + String(hostname) + ".local");
  }
  Serial.println("========================================\n");
}

// ---- Setup ----
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\nESP32 Gaszähler Gateway v1.0");
  Serial.println("================================");
  
  // Status LED
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);
  
  // Reset Button konfigurieren
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  
  // Prüfen ob BOOT-Button beim Start gedrückt ist (LOW = gedrückt)
  if (digitalRead(RESET_BUTTON_PIN) == LOW) {
    Serial.println("\n*** CONFIG RESET ERKANNT ***");
    Serial.println("BOOT-Button war beim Start gedrückt.");
    Serial.println("Lösche gespeicherte Konfiguration...");
    
    preferences.begin("gas-config", false);
    preferences.clear();
    preferences.end();
    
    Serial.println("Konfiguration gelöscht!");
    Serial.println("Starte im Access Point Modus...\n");
    
    // Defaults setzen
    strcpy(ssid, "SSID");
    strcpy(password, "Password");
    strcpy(hostname, "ESP32-GasZaehler");
    strcpy(mqtt_server, "192.168.178.1");
    mqtt_port = 1883;
    strcpy(mqtt_topic, "gaszaehler/verbrauch");
    poll_interval = 60000;
    
    delay(1000); // Warten damit Button losgelassen werden kann
  }
  
  loadConfig();
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
  client.setBufferSize(512); // Größerer Buffer für Discovery
  
  // Client-ID mit MAC-Adresse für Eindeutigkeit
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
  
  Serial.println("Setup abgeschlossen!");
  Serial.println("================================\n");
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
  
  unsigned long now = millis();
  
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
          
          // Hex Dump speichern
          mbusStats.lastHexDump = "";
          for (size_t i = 0; i < min(mbusLen, (size_t)32); i++) {
            char hex[4];
            sprintf(hex, "%02X ", mbusBuffer[i]);
            mbusStats.lastHexDump += hex;
          }

          float volume = parseGasVolumeBCD(mbusBuffer, mbusLen);
          if (volume >= 0) {
            mbusStats.successfulPolls++;
            char payload[16];
            dtostrf(volume, 0, 2, payload);
            
            if (client.publish(mqtt_topic, payload)) {
              Serial.print("Verbrauch gesendet: ");
              Serial.println(payload);
              
              // Additional HA sensors
              String wifiTopic = String(mqtt_topic) + "_wifi";
              client.publish(wifiTopic.c_str(), String(WiFi.RSSI()).c_str());
              
              String rateTopic = String(mqtt_topic) + "_mbus_rate";
              float rate = mbusStats.totalPolls > 0 ? (mbusStats.successfulPolls * 100.0 / mbusStats.totalPolls) : 0;
              client.publish(rateTopic.c_str(), String(rate, 1).c_str());
            } else {
              errorStats.mqttErrors++;
              logError("MQTT Publish fehlgeschlagen");
            }
            
            // Verlauf speichern mit echter Zeit wenn verfügbar
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
        mbusState = MBUS_IDLE; // wieder bereit für nächsten Poll
      }
      break;
  }
}
