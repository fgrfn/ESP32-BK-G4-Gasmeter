# ESP32 BK-G4 Gas Meter

ESP32 gateway for reading the M-Bus encoder of a Honeywell/Elster BK-G4 gas meter and publishing the values to MQTT and Home Assistant.

> Based on the original `BK-G4AT2MQTT` project by BennoB666. Version 3 replaces the previous parallel legacy/module implementation with one tested modular firmware.

## Features

- Validated M-Bus long-frame parser with length, stop-byte, checksum and BCD checks
- Total volume, energy, flow, daily/monthly/yearly consumption and tariff-aware costs
- MQTT with retained JSON state, Last Will, optional TLS and stable per-device Home Assistant discovery IDs
- Optional MQTT commands for polling, restart, calorific value and correction factor
- Responsive local WebUI, Prometheus metrics, logs, diagnostics and protected OTA upload
- Random setup-AP password, authenticated configuration routes and redacted secrets
- Config export/import, schema migration from the old `gas-config` namespace and meter offset support
- Safe mode after repeated failed boots and ESP-IDF OTA rollback validation
- Captive setup portal, mDNS/hostname support and static IPv4 configuration
- Native parser/core tests, production/debug builds and tag-based release artifacts

## Hardware

| ESP32 | M-Bus interface |
|---|---|
| GPIO17 / TX1 | RX |
| GPIO16 / RX1 | TX |
| 5V | VCC |
| GND | GND |

UART parameters: **2400 baud, 8E1**. Verify that the M-Bus adapter provides the correct electrical level and isolation for your meter.

## First installation

```bash
git clone https://github.com/fgrfn/ESP32-BK-G4-Gasmeter.git
cd ESP32-BK-G4-Gasmeter
pio run -e esp32dev -t upload
pio device monitor
```

On first boot, the firmware opens an access point named `ESP32-Gas-XXXXXX`. Its random password and the generated WebUI/OTA credentials are printed to the serial monitor. Open `http://192.168.4.1` and enter Wi-Fi and MQTT settings.

Holding the ESP32 BOOT button for at least three seconds during startup performs a physical factory reset.

## Home Assistant

Enable MQTT discovery in Home Assistant. The device publishes a retained JSON state under:

```text
<base_topic>/state
<base_topic>/diagnostics
<base_topic>/availability
```

Discovery identifiers contain the ESP32 MAC suffix, so multiple meters do not collide. Gas energy uses `total_increasing` and can be selected in the Energy Dashboard.

## Security

The dashboard status and Prometheus endpoint are read-only. Configuration, logs, manual polling, reset, restart and OTA are authenticated outside setup-AP mode. Passwords are never returned by `/api/config`.

Do not expose the HTTP WebUI directly to the Internet. See [SECURITY.md](SECURITY.md) for TLS, Secure Boot, signed releases and network-segmentation guidance.

## Build and test

```bash
pio test -e native
pio run -e esp32dev
pio run -e esp32dev-debug
pio check -e esp32dev --fail-on-defect high
```

Dependencies and the ESP32 platform are pinned in `platformio.ini`. The Async libraries use the maintained `ESP32Async` repositories rather than the archived `me-no-dev` repositories.

## Releases

A release is created only for a semantic version tag matching `include/version.h`:

```bash
git tag v3.0.0
git push origin v3.0.0
```

The workflow publishes:

- `*-ota.bin` for WebUI/Arduino OTA
- `*-factory.bin` containing bootloader, partition table and application
- `SHA256SUMS`
- optional `*-ota-signed.bin` when the repository secret `ESP_SIGNING_KEY` is configured

## API overview

| Endpoint | Purpose | Authentication |
|---|---|---|
| `GET /api/status` | Live values and health | No |
| `GET /metrics` | Prometheus text format | No |
| `GET /api/config` | Redacted configuration | Yes outside setup AP |
| `POST /api/config` | Validate, save and restart | Yes outside setup AP |
| `GET /api/config/export` | Export without secrets | Yes |
| `POST /api/mbus/poll` | Trigger a poll | Yes |
| `GET /api/logs` | Ring-buffer logs | Yes |
| `POST /api/ota` | Firmware upload | Yes |
| `POST /api/factory-reset` | Reset with `{"confirm":"RESET"}` | Yes, never setup bypass |

## License

See [LICENSE](LICENSE).
