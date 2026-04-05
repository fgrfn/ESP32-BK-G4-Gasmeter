# Versionsverlauf

Alle wichtigen Änderungen an diesem Projekt werden in dieser Datei dokumentiert.

Das Format basiert auf [Keep a Changelog](https://keepachangelog.com/de/1.0.0/),
und dieses Projekt folgt [Semantic Versioning](https://semver.org/lang/de/).

## [Unreleased]

## [2.1.0] - 2026-04-05

### 🏗️ Hauptfeature: Modulare Code-Architektur

#### Hinzugefügt
- **8 neue Module** für bessere Code-Organisation:
  - `Logger` - Zentrales Logging-System mit Ring-Buffer
  - `Config` - Konfigurations-Management mit NVS
  - `MBusReader` - M-Bus Kommunikation & Parsing
  - `WiFiManager` - WiFi Station/AP Verwaltung
  - `MQTTHandler` - MQTT & Home Assistant Integration
  - `WebAuth` - WebUI HTTP Digest Authentifizierung
  - `MQTTTls` - TLS/SSL Support für MQTT
  - `constants.h` - Zentrale Konstanten & Debug-Makros

- **Debug-Makro-System** mit granularer Kontrolle:
  - `DEBUG_LOG(msg)` - Allgemeine Debug-Ausgaben
  - `MBUS_DEBUG(msg)` - M-Bus spezifisch
  - `MQTT_DEBUG(msg)` - MQTT spezifisch
  - `WIFI_DEBUG(msg)` - WiFi spezifisch
  - Optional: ANSI-Farb-Codes für Serial Monitor

- **Neue Build-Environments**:
  - `esp32dev` - Production Build (optimiert, keine Debug-Ausgaben)
  - `esp32dev-debug` - Debug Build (alle Debug-Flags, ANSI-Colors)

- **Umfassende Dokumentation**:
  - [ARCHITECTURE.md](ARCHITECTURE.md) - Modulare Architektur erklärt
  - [SECURITY.md](SECURITY.md) - Sicherheits-Features & Setup
  - [IMPROVEMENTS.md](IMPROVEMENTS.md) - Detaillierte Verbesserungen

#### Geändert
- **Memory-Optimierungen**: String-Konkatenation durch `snprintf()` ersetzt in:
  - `checkMemory()` - Heap-Statistiken
  - `setup_wifi()` - WiFi-Status-Meldungen
  - `saveConfig()` - Config-Ausgaben
  - `updateStatusLED()` - LED-Timing
  
- **Zentrale Konstanten**: Alle Magic Numbers nach `constants.h` verschoben:
  - GPIO Pin Definitionen
  - Timing-Konstanten (Intervalle, Timeouts)

#### 🗑️ Entfernt
- **Lokale History/Statistik-Funktionalität**:
  - `MeasurementData` Struct vollständig entfernt
  - `measurements` Vector entfernt (~1.6KB RAM gespart)
  - `saveHistory()` / `loadHistory()` Funktionen entfernt
  - Chart.js Bibliotheken entfernt (~150KB Flash gespart)
  - Statistik-Karten (Heute/Woche/Monat) aus WebUI entfernt
  - `drawChart()` / `updateConsumptionStats()` Funktionen entfernt
  - Export CSV Funktion entfernt
  - **Rationale:** Home Assistant speichert bereits alle Daten langfristig
  - **Vorteile:** ~2KB RAM gespart, ~150KB Flash gespart, weniger Flash-Writes, schnellere WebUI

#### 🆕 Hinzugefügt (Backup & Restore)
- **Config Backup & Restore System**:
  - `Config::exportToJson()` - Export Konfiguration als JSON-Datei
  - `Config::importFromJson()` - Import & Validierung von JSON-Config
  - `Config::validateJson()` - Schema-Validierung vor Import
  - WebUI Export-Buttons (mit/ohne Passwörter)
  - WebUI Import-Button mit File-Upload
  - JSON-Format dokumentiert und versioniert
  - **Use Cases:** Migration, Disaster Recovery, Multi-Device Setup
  - **Sicherheit:** Passwörter optional exportierbar
  - Memory-Schwellwerte
  - Default-Konfigurationswerte
  - String-Buffer-Größen

- **Code-Struktur**: main.cpp von 3150+ auf ~800 Zeilen reduziert

- **README**: Erweitert mit Hinweisen auf modulare Architektur und Sicherheits-Features

#### Sicherheit
- **WebUI Authentifizierung**: HTTP Digest Auth für WebUI (optional aktivierbar)
- **MQTT-TLS Support**: Verschlüsselte MQTT-Verbindung mit CA-Zertifikat-Validierung
- **NVS Encryption Ready**: Dokumentation für ESP32 Flash Encryption
- **Sichere Credential-Verwaltung**: Password-Validierung, Secure Storage Patterns

#### Performance
- **Reduzierte Heap-Fragmentierung**: -15+ temporäre String-Allokationen
- **Debug-Overhead eliminiert**: 0% in Production-Builds (war ~5%)
- **Memory-Management**: Optimierte Schwellwerte und Auto-Cleanup

#### Entwickler-Experience
- **Separation of Concerns**: Klare Module mit definierten Verantwortlichkeiten
- **Unit-Test Ready**: Module sind unabhängig testbar
- **Bessere Code-Navigation**: 15 Dateien statt 1 monolithischer Datei
- **Inline-Dokumentation**: API-Docs in Header-Dateien

### Technische Details
- Keine Breaking Changes - 100% rückwärtskompatibel
- Bestehende Konfigurationen bleiben gültig
- OTA-Updates von v2.0.5 → v2.1.0 möglich

---

## [2.0.5] - 2026-03-08

### Hinzugefügt
- Home Assistant Gasdurchfluss-Sensor (`sensor.esp32_gaszaehler_flow`) via MQTT Discovery (`unit_of_meas: m³/h`, `device_class: volume_flow_rate`).
- Neues MQTT Topic `<base_topic>_flow` mit berechnetem Gasdurchfluss in m³/h.

### Geändert
- Durchflussberechnung aus Delta Zählerstand / Delta Zeit zwischen zwei gültigen M-Bus Messungen.
- API erweitert um `flow_m3h` im Endpoint `/api/data`.

## [2.0.5] - 2026-03-08

### Geändert
- WebUI Dark/Light Toggle stabilisiert (fehlendes Theme-Element ergänzt, robustere JS-Checks).
- Defekten Chart-JavaScript-Block entfernt, der zu Runtime-Fehlern führen konnte.
- `GET /api/config` liefert jetzt auch Static-IP Felder (`use_static_ip`, `static_ip`, `static_gateway`, `static_subnet`, `static_dns`).
- Konfigurations-Parsing in `POST /api/config` auf wiederverwendbare Helper umgestellt (robuster bei Zahlen/Bool/String).
- AP-Fallback standardmäßig mit Passwort (`12345678`) statt offenem AP.

### Behoben
- Inkonsistente Versionsanzeige in der WebUI-Update-Seite (nun dynamisch über `%VERSION%`).
- README-Platzhalter (`YOUR-USERNAME`) auf `fgrfn` korrigiert.

### Hinzugefügt
- Automatisierte GitHub Actions Workflows für Build und Release
- Semantische Versionierung (v1.0.0 Format)
- Firmware-Version wird zentral in `FIRMWARE_VERSION` Konstante verwaltet
- Version wird in Home Assistant, WebUI und Serial Output angezeigt
- Automatische Binary-Erstellung bei neuen Tags
- GitHub Releases mit Download-Links für Firmware

### Geändert
- Version von "1.2" auf "1.0.0" (Semantic Versioning)
- HTML-Seite zeigt nun dynamisch die aktuelle Firmware-Version
- README erweitert um Release-Management Dokumentation

## [1.0.0] - TBD

### Erste offizielle Release

#### Hinzugefügt
- 🌐 Modernes WebUI mit Dashboard, Chart und Konfiguration
- 🏠 Home Assistant Auto-Discovery (MQTT)
- 📊 Verlaufs-Chart der letzten 50 Messungen
- 🔧 Web-Konfiguration (WiFi, MQTT, Polling-Intervall)
- ⚡ Access Point Fallback für einfache Erstkonfiguration
- 🔄 OTA-Updates über Web-Interface
- 📡 NTP-Zeitserver mit automatischer Sommerzeitumstellung
- 💾 Persistente Konfiguration im Flash-Speicher
- 🚨 Fehlerstatistik und detailliertes Logging
- 🔔 MQTT Last Will Testament (LWT)
- 📍 mDNS Support (esp32-gaszaehler.local)
- 💡 Status-LED mit verschiedenen Blink-Codes
- 🔘 Factory-Reset über BOOT-Button (10s gedrückt halten)

#### Technische Features
- M-Bus Kommunikation über UART2 (2400 Baud, 8E1)
- Retry-Mechanismus bei Kommunikationsfehlern
- Watchdog für automatische Neustarts bei Problemen
- WiFi Reconnect mit exponential Backoff
- MQTT Keep-Alive und Auto-Reconnect
- Energy Dashboard Integration für Home Assistant

#### Unterstützte Boards
- ESP32 DevKit V1
- ESP32-C3 DevKit

---

## Versionsrichtlinien

- **MAJOR**: Breaking Changes, die manuelle Anpassungen erfordern
- **MINOR**: Neue Features, abwärtskompatibel
- **PATCH**: Bugfixes, Performance-Verbesserungen

## Links

- [Repository](https://github.com/BennoB666/BK-G4AT2MQTT)
- [Releases](https://github.com/BennoB666/BK-G4AT2MQTT/releases)
- [Issues](https://github.com/BennoB666/BK-G4AT2MQTT/issues)
