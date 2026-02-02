# Release Management

Diese Anleitung beschreibt, wie neue Versionen erstellt und veröffentlicht werden.

## Semantic Versioning

Wir verwenden [Semantic Versioning](https://semver.org/lang/de/) im Format `MAJOR.MINOR.PATCH`:

- **MAJOR** (1.x.x): Breaking Changes, die manuelle Anpassungen erfordern
- **MINOR** (x.1.x): Neue Features, vollständig abwärtskompatibel
- **PATCH** (x.x.1): Bugfixes, kleinere Verbesserungen

## Release-Prozess

### 1. Version erhöhen

Editiere [src/main.cpp](src/main.cpp) und aktualisiere die `FIRMWARE_VERSION`:

```cpp
const char* FIRMWARE_VERSION = "1.1.0";  // Neue Version
```

### 2. CHANGELOG aktualisieren

Editiere [CHANGELOG.md](CHANGELOG.md) und dokumentiere die Änderungen:

```markdown
## [1.1.0] - 2026-02-02

### Hinzugefügt
- Neue Feature X
- Unterstützung für Y

### Geändert
- Verbesserte Performance bei Z

### Behoben
- Bug beim Laden der Konfiguration
```

### 3. Änderungen committen

```bash
git add src/main.cpp CHANGELOG.md
git commit -m "Bump version to 1.1.0"
```

### 4. Tag erstellen und pushen

```bash
# Tag erstellen
git tag v1.1.0

# Tag und Commits pushen
git push origin main
git push origin v1.1.0
```

### 5. Automatisches Release

Nach dem Push des Tags:

1. ✅ GitHub Actions startet automatisch
2. 🔨 Firmware wird kompiliert (`BK-G4AT2MQTT-1.1.0.bin`)
3. 📦 GitHub Release wird erstellt
4. ⬇️ Binary ist zum Download verfügbar

## Workflow-Übersicht

### Build Workflow (`.github/workflows/build.yml`)

- **Trigger**: Push auf `main` oder `develop`, Pull Requests
- **Zweck**: Code kompilieren und testen
- **Artefakte**: Firmware-Binary (7 Tage Aufbewahrung)

### Release Workflow (`.github/workflows/release.yml`)

- **Trigger**: Push von Tags im Format `v*.*.*`
- **Zweck**: Offizielle Releases erstellen
- **Ausgabe**: 
  - Kompilierte Firmware-Binary
  - GitHub Release mit Installations-Anleitung
  - Changelog-Integration

## Lokale Tests vor Release

### 1. Lokalen Build testen

```bash
# PlatformIO installieren
pip install platformio

# Projekt kompilieren
pio run

# Binary befindet sich in:
# .pio/build/esp32dev/firmware.bin
```

### 2. Auf Test-Device flashen

```bash
# Via USB
pio run --target upload

# Oder manuell
esptool.py --port /dev/ttyUSB0 write_flash 0x10000 .pio/build/esp32dev/firmware.bin
```

### 3. Funktionalität testen

- [ ] WebUI erreichbar
- [ ] Version wird korrekt angezeigt
- [ ] M-Bus Daten werden ausgelesen
- [ ] MQTT funktioniert
- [ ] OTA-Update funktioniert
- [ ] Home Assistant Discovery

## Hotfix-Release

Für dringende Bugfixes auf der aktuellen Version:

```bash
# Patch-Version erhöhen (1.0.0 → 1.0.1)
# In src/main.cpp: FIRMWARE_VERSION = "1.0.1"

git add src/main.cpp CHANGELOG.md
git commit -m "Fix: Critical bug in M-Bus parsing"
git tag v1.0.1
git push origin main --tags
```

## Pre-Release / Beta

Für Test-Versionen:

```bash
# Beta-Tag erstellen
git tag v1.1.0-beta.1
git push origin v1.1.0-beta.1
```

Im Release Workflow wird automatisch erkannt, ob es sich um eine Prerelease handelt.

## Rollback

Falls ein Release Probleme verursacht:

```bash
# Vorherige Version als neue Version taggen
git tag v1.0.2 v1.0.0
git push origin v1.0.2
```

Nutzer können dann auf die stabile Version downgraden.

## Troubleshooting

### GitHub Actions schlägt fehl

1. Überprüfe die Logs im "Actions" Tab
2. Häufige Probleme:
   - PlatformIO-Bibliotheken nicht gefunden → Cache löschen
   - Kompilierfehler → Lokal testen mit `pio run`
   - Tag-Format falsch → Muss `v*.*.*` sein

### Release wird nicht erstellt

- Prüfe, ob der Tag mit `v` beginnt (z.B. `v1.0.0`, nicht `1.0.0`)
- Prüfe GitHub Actions Permissions in den Repository-Settings

## Checkliste für Releases

- [ ] Version in `src/main.cpp` erhöht
- [ ] CHANGELOG.md aktualisiert
- [ ] Lokaler Build erfolgreich
- [ ] Auf Test-Gerät getestet
- [ ] Commit erstellt
- [ ] Tag mit korrektem Format erstellt (`v*.*.*`)
- [ ] Tag und Commits gepusht
- [ ] GitHub Actions erfolgreich durchgelaufen
- [ ] Release auf GitHub sichtbar
- [ ] Binary downloadbar und funktionsfähig

## Fragen?

Bei Fragen oder Problemen öffne ein Issue auf GitHub.
