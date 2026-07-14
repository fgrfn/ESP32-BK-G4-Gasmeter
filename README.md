# ESP32 BK-G4 Gas Meter

ESP32 gateway for reading the M-Bus encoder of a Honeywell/Elster BK-G4 gas meter and publishing reliable volume, energy and diagnostic values to MQTT and Home Assistant.

> Based on the original `BK-G4AT2MQTT` project by BennoB666. Version 3 replaces the previous parallel legacy/module implementation with one tested modular firmware.

## Features

- Validated M-Bus long-frame parser with length, stop-byte, checksum and BCD checks
- Total volume and energy, flow, daily/monthly/yearly volume and energy
- 31-day completed-day history with CSV export
- Continuous-flow plausibility warning with configurable threshold and duration
- MQTT with retained JSON state, Last Will, optional TLS and unique per-device base topics
- Home Assistant discovery with diagnostics, health sensors and optional poll/restart/configuration controls
- Responsive local dashboard with a mechanical gas-meter-style counter display
- Full local WebUI for network, MQTT, measurement and security settings
- Config import/export, logs, M-Bus hex dump, Prometheus metrics and manual polling
- Random setup-AP and ArduinoOTA passwords, HTTP Digest authentication and redacted secrets
- Editable WebUI credentials with documented initial login
- NTP-aware accounting: period values are not updated until the clock is synchronized
- Reset-reason-aware safe mode and delayed ESP-IDF OTA rollback acceptance
- Captive setup portal, mDNS/hostname support and static IPv4 configuration
- Native parser/core/time-boundary tests, deterministic fuzz coverage and capture replay tooling

Gas-price and cost forecasts are intentionally not implemented in the firmware. They can be created in Home Assistant from the energy or volume entities. See [docs/HOME_ASSISTANT.md](docs/HOME_ASSISTANT.md).

## Hardware

Default wiring:

| ESP32 | M-Bus interface |
|---|---|
| GPIO17 / TX1 | RX |
| GPIO16 / RX1 | TX |
| GND | GND when required by the adapter |

UART parameters: **2400 baud, 8E1**.

Never connect an ESP32 UART directly to the M-Bus line. Use a suitable M-Bus interface. Pin overrides, commissioning and capture instructions are documented in [docs/HARDWARE.md](docs/HARDWARE.md).

## First installation

```bash
git clone https://github.com/fgrfn/ESP32-BK-G4-Gasmeter.git
cd ESP32-BK-G4-Gasmeter
pio run -e esp32dev -t upload
pio device monitor
```

On first boot, the firmware opens an access point named `ESP32-Gas-XXXXXX`. The setup-AP password and the generated ArduinoOTA password are printed to the serial monitor. Open `http://192.168.4.1` and enter Wi-Fi and MQTT settings.

Initial WebUI login:

```text
Username: admin
Password: admin
```

Change both values after the first login under **Configuration → Security**. The new username and password are stored in NVS and remain active across normal restarts and OTA updates.

Firmware 3.1.1 uses configuration schema 5. When upgrading an older schema, the WebUI login is reset once to `admin` / `admin`; credentials changed afterwards are preserved.

Holding the ESP32 BOOT button for at least three seconds during startup performs a physical factory reset. A factory reset also restores the WebUI login to `admin` / `admin`.

## WebUI dashboard

The dashboard is embedded directly in the firmware and does not load external fonts, scripts or cloud resources. It is designed for desktop and mobile browsers and includes:

- a mechanical counter-style representation of the gas meter reading
- black whole-number rollers and red decimal rollers, matching a physical gas meter
- live status indicators for M-Bus, NTP and MQTT
- total energy, flow rate and M-Bus success rate
- daily, monthly and yearly volume and energy values
- continuous-flow warning state
- collapsible configuration, import/export and diagnostics sections

The browser refreshes live values every five seconds. The dashboard and metrics endpoints are read-only; configuration and maintenance actions require authentication outside setup-AP mode.

## Home Assistant and MQTT

The default base topic includes the ESP32 device ID, preventing collisions between multiple meters:

```text
gasmeter/<device_id>/state
gasmeter/<device_id>/diagnostics
gasmeter/<device_id>/availability
```

Existing installations that still use the former default topic `gasmeter` are migrated automatically. Explicitly configured custom topics are preserved.

Enable MQTT commands only on a trusted broker. They create Home Assistant controls for polling, restarting and changing selected measurement settings. Details and a manual gas-cost example are in [docs/HOME_ASSISTANT.md](docs/HOME_ASSISTANT.md).

## Firmware updates

The non-functional browser firmware-upload endpoint was removed. Supported update paths are:

- serial/USB upload through PlatformIO or esptool
- ArduinoOTA using the generated/configured OTA password
- a factory image from a tagged GitHub release

A pending ESP-IDF OTA image is accepted only after the device has passed runtime health checks. Safe mode never accepts a pending image.

## Security

The dashboard status and Prometheus endpoint are read-only. Configuration, logs, history export, manual polling, reset and restart are authenticated outside setup-AP mode. Passwords are never returned by `/api/config`.

The initial `admin` / `admin` login is intentionally predictable for commissioning and is not suitable for permanent operation. Change it immediately and do not expose the HTTP WebUI directly to the Internet. See [SECURITY.md](SECURITY.md) for TLS, Secure Boot and network-segmentation guidance.

## Build and test

```bash
pio test -e native
pio run -e esp32dev
pio run -e esp32dev-debug
pio check -e esp32dev --fail-on-defect high
python tools/replay_mbus.py test/fixtures/bk_g4_synthetic.hex --expect 123.456
```

Dependencies and the ESP32 platform are pinned in `platformio.ini`. GitHub Actions performs a weekly PlatformIO dependency audit; `renovate.json` can be used when the Renovate app is installed.

## Releases

A release is created only for a semantic version tag matching `include/version.h`:

```bash
git tag v3.1.1
git push origin v3.1.1
```

The workflow publishes:

- `*-ota.bin` for ArduinoOTA/external OTA tooling
- `*-factory.bin` containing bootloader, partition table and application
- `SHA256SUMS`
- optional `*-ota-signed.bin` when `ESP_SIGNING_KEY` is configured

## API overview

| Endpoint | Purpose | Authentication |
|---|---|---|
| `GET /api/status` | Live values and health | No |
| `GET /metrics` | Prometheus text format | No |
| `GET /api/config` | Redacted configuration | Yes outside setup AP |
| `POST /api/config` | Validate, save and restart | Yes outside setup AP |
| `GET /api/config/export` | Export configuration | Yes |
| `GET /api/usage.csv` | Completed daily history | Yes |
| `POST /api/mbus/poll` | Trigger a poll | Yes |
| `GET /api/logs` | Ring-buffer logs | Yes |
| `POST /api/restart` | Planned restart | Yes |
| `POST /api/factory-reset` | Reset with `{"confirm":"RESET"}` | Yes, never setup bypass |

## License

See [LICENSE](LICENSE).
