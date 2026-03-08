# Versionsverlauf

Alle wichtigen Änderungen an diesem Projekt werden in dieser Datei dokumentiert.

Das Format basiert auf [Keep a Changelog](https://keepachangelog.com/de/1.0.0/),
und dieses Projekt folgt [Semantic Versioning](https://semver.org/lang/de/).

## [Unreleased]

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
