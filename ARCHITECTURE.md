# 🏗️ Projekt-Architektur

## Übersicht

Das Projekt wurde modularisiert für bessere Wartbarkeit und Testbarkeit.

## Verzeichnisstruktur

```
BK-G4AT2MQTT/
├── include/
│   ├── constants.h          # Zentrale Konstanten & Defines
│   ├── Logger.h             # Log-System
│   ├── Config.h             # Konfigurations-Management
│   ├── MBusReader.h         # M-Bus Kommunikation
│   ├── WiFiManager.h        # WiFi-Verwaltung
│   └── MQTTHandler.h        # MQTT & Home Assistant
├── src/
│   ├── main.cpp             # Haupt-Programmlogik
│   ├── Logger.cpp
│   ├── Config.cpp
│   ├── MBusReader.cpp
│   ├── WiFiManager.cpp
│   └── MQTTHandler.cpp
├── platformio.ini           # Build-Konfiguration
└── README.md
```

## Module

### 1. constants.h
**Zweck:** Zentrale Definition aller Konstanten und Makros

**Enthält:**
- GPIO Pin Definitionen (LED, M-Bus UART, Buttons)
- Timing-Konstanten (Intervalle, Timeouts)
- Memory-Schwellwerte
- Default-Konfigurationswerte
- Debug-Makros (DEBUG_LOG, MBUS_DEBUG, MQTT_DEBUG, WIFI_DEBUG)
- ANSI-Farb-Codes (optional)

**Verwendung:**
```cpp
#include "constants.h"

// Debug-Ausgaben (nur wenn DEBUG_MODE definiert)
DEBUG_LOG("Starte System...");
MBUS_DEBUG("Poll gesendet");

// Konstanten verwenden
if (freeHeap < MEMORY_WARNING_THRESHOLD) {
  // Warnung ausgeben
}
```

### 2. Logger (Logger.h/cpp)
**Zweck:** Zentralisiertes Logging-System

**Features:**
- Ring-Buffer für Web-UI (letzte N Log-Einträge)
- Automatische ANSI-Code-Filterung
- Zeitstempel (NTP oder millis())
- Serial Monitor Output

**API:**
```cpp
Logger::init();
Logger::addLog("System gestartet");
Logger::setTimeInitialized(true);
const std::vector<LogEntry>& logs = Logger::getLogBuffer();
Logger::clearLogs();
```

### 3. Config (Config.h/cpp)
**Zweck:** Konfigurationsverwaltung mit NVS-Persistence

**Verwaltet:**
- WiFi-Einstellungen (SSID, Passwort, Static IP)
- MQTT-Einstellungen (Server, Port, Credentials, Topics)
- Gas-Parameter (Brennwert, Z-Zahl, Poll-Intervall)
- Deep-Sleep Konfiguration

**API:**
```cpp
Config::init();           // Defaults setzen
Config::load();           // Aus NVS laden
Config::save();           // In NVS speichern
Config::generateMqttClientId();

// Zugriff auf Parameter
const char* ssid = Config::ssid;
unsigned long interval = Config::poll_interval;
```

### 4. MBusReader (MBusReader.h/cpp)
**Zweck:** M-Bus Kommunikation & Daten-Parsing

**Features:**
- State Machine (IDLE / WAIT_RESPONSE)
- Statistiken (Erfolgsrate, Response-Zeit)
- BCD-Parsing für Gasvolumen
- Hex-Dump für Debugging

**API:**
```cpp
MBusReader::init();
MBusReader::poll();  // Poll-Frame senden

float volume;
bool success;
MBusReader::processResponse(volume, success);

MBusStats& stats = MBusReader::getStats();
Serial.println(stats.avgResponseTime);
```

### 5. WiFiManager (WiFiManager.h/cpp)
**Zweck:** WiFi-Verbindungsverwaltung

**Features:**
- Station Mode (Client)
- Access Point Mode (bei Fehler/Ersteinrichtung)
- Static IP Support
- Auto-Reconnect

**API:**
```cpp
WiFiManager::init();
bool connected = WiFiManager::connect();

if (WiFiManager::isAPMode()) {
  // AP-Modus aktiv
}

WiFiManager::checkConnection();  // Reconnect wenn nötig
String ip = WiFiManager::getLocalIP();
int rssi = WiFiManager::getRSSI();
```

### 6. MQTTHandler (MQTTHandler.h/cpp)
**Zweck:** MQTT-Kommunikation & Home Assistant Integration

**Features:**
- Automatisches Connect mit Last Will Testament
- Home Assistant MQTT Discovery
- Sensor Publishing (Volume, Energy, Flow, etc.)
- Error Statistics

**API:**
```cpp
WiFiClient espClient;
MQTTHandler::init(espClient);
MQTTHandler::connect();
MQTTHandler::loop();

// Publishing
MQTTHandler::publishVolume(8451.83);
MQTTHandler::publishEnergy(87632.1);
MQTTHandler::publishFlow(0.125);

// Home Assistant Discovery
MQTTHandler::sendHomeAssistantDiscovery();

// Error Tracking
ErrorStats& stats = MQTTHandler::getErrorStats();
```

## Build-Konfiguration

### Production Build
```bash
pio run && pio run -t upload
```
- Keine Debug-Ausgaben
- Optimierte Binary-Größe

### Debug Build
```bash
pio run -e esp32dev-debug && pio run -e esp32dev-debug -t upload
```
- Alle Debug-Ausgaben aktiviert
- ANSI-Farben im Serial Monitor
- Detaillierte M-Bus/MQTT/WiFi Logs

### Selective Debug
In `include/constants.h` editieren:
```cpp
#define DEBUG_MODE      // Alle Debug-Logs
#define DEBUG_MBUS      // Nur M-Bus
// #define DEBUG_MQTT   // Optional
// #define DEBUG_WIFI   // Optional
#define ENABLE_ANSI_COLORS  // Farbiger Serial Output
```

## Dependency Graph

```
main.cpp
  ├── Logger
  ├── Config
  │     └── Logger
  ├── WiFiManager
  │     ├── Config
  │     └── Logger
  ├── MQTTHandler
  │     ├── Config
  │     └── Logger
  └── MBusReader
        └── Logger
```

## Memory Management

**Maßnahmen gegen Fragmentierung:**
- `snprintf()` statt String-Konkatenation für kritische Stellen
- Statische Buffers wo möglich
- Ring-Buffer mit fester Größe
- Memory Check alle 60s mit Auto-Cleanup

**Schwellwerte:**
- `MEMORY_WARNING_THRESHOLD`: 10 KB - Warnung
- `MEMORY_CLEANUP_THRESHOLD`: 5 KB - Aggressives Cleanup
- `MEMORY_CRITICAL_THRESHOLD`: 3 KB - Emergency Restart

## Erweiterbarkeit

### Neuen Sensor hinzufügen

1. **MBusReader erweitern:**
   ```cpp
   float parseNewValue(const uint8_t* data, size_t len);
   ```

2. **MQTTHandler erweitern:**
   ```cpp
   bool publishNewSensor(float value);
   void sendDiscoveryForNewSensor();
   ```

3. **Config erweitern (falls nötig):**
   ```cpp
   static float new_parameter;
   ```

### Neue Konfiguration hinzufügen

1. In `Config.h` deklarieren:
   ```cpp
   static bool new_feature_enabled;
   ```

2. In `Config.cpp` initialisieren und laden/speichern

3. In WebUI Config-Seite hinzufügen

## Best Practices

✅ **DO:**
- Nutze `DEBUG_LOG()` Makros für Debug-Ausgaben
- Verwende `snprintf()` für String-Formatierung
- Definiere Magic Numbers in `constants.h`
- Halte Module unabhängig (nur Logger-Dependency)
- Dokumentiere breaking Changes im CHANGELOG

❌ **DON'T:**
- Keine direkten `Serial.println("DEBUG: ...")` mehr
- Keine hardcoded Timeouts/Pins/Werte
- Keine String-Konkatenation in Loops
- Keine globalen Variablen ohne `static` in Klassen

## Testing

### Unit-Tests (planned)
```bash
pio test
```

Tests für:
- `MBusReader::parseGasVolumeBCD()`
- `Config::validatePollInterval()`
- Memory-Management Funktionen

## Migration Guide

**Von alter main.cpp zur modularen Struktur:**

Alt:
```cpp
void setup() {
  loadConfig();
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  mbusSerial.begin(MBUS_BAUD, ...);
}
```

Neu:
```cpp
void setup() {
  Logger::init();
  Config::init();
  Config::load();
  WiFiManager::init();
  WiFiManager::connect();
  MQTTHandler::init(espClient);
  MQTTHandler::connect();
  MBusReader::init();
}
```

## Version History

**v2.0.5 → v2.1.0 (Module Refactoring)**
- Code in 6 Module aufgeteilt
- `constants.h` für zentrale Definitionen
- Debug-Makro-System implementiert
- String-Operationen optimiert
- Production/Debug Build Environments

## Lizenz

Siehe [LICENSE](../LICENSE)
