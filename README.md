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
- Tabbed WebUI for overview, WLAN, MQTT, measurement, security and system diagnostics
- Config import/export, logs, M-Bus hex dump, Prometheus metrics and manual polling
- Random setup-AP password, optional HTTP Digest authentication and redacted secrets
- Admin login disabled by default, with editable WebUI credentials when enabled
- Optional ArduinoOTA password that can be added or removed through the WebUI
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

On first boot, the firmware opens an access point named `ESP32-Gas-XXXXXX`. The randomly generated setup-AP password is printed to the serial monitor. Open `http://192.168.4.1` and enter Wi-Fi and MQTT settings.

The WebUI admin login is disabled by default. It can be enabled under **Sicherheit**. The initial credentials are:

```text
Username: admin
Password: admin
```

Change both values when enabling the login. The setting, username and password are stored in NVS and remain active across normal restarts and OTA updates.

Firmware 3.1.2 uses configuration schema 7. Fresh installations and factory resets start with the admin login disabled. Existing installations keep authentication enabled during the upgrade so their previous protection and credentials are preserved. Upgrading from a schema older than 6 clears the former automatically generated ArduinoOTA password once.

Holding the ESP32 BOOT button for at least three seconds during startup performs a physical factory reset. A factory reset disables the WebUI login, restores the stored credentials to `admin` / `admin` for optional later activation and leaves the ArduinoOTA password empty.

## WebUI dashboard

The dashboard is embedded directly in the firmware and does not load external fonts, scripts or cloud resources. It is designed for desktop and mobile browsers.

The top navigation contains separate tabs for:

- **Übersicht** with the mechanical gas-meter display and live consumption values
- **WLAN** with DHCP/static IPv4, hostname and timezone settings
- **MQTT** with broker, topic, Home Assistant and TLS settings
- **Messung** with polling, offset, energy conversion and continuous-flow warning
- **Sicherheit** with WebUI credentials and optional ArduinoOTA authentication
- **System** with import/export, M-Bus polling, diagnostics, logs, restart and factory reset

The meter reading uses black whole-number rollers and red decimal rollers like a physical gas meter. Live values refresh every five seconds. The dashboard and metrics endpoints are read-only. When the optional admin login is enabled, configuration and maintenance actions require authentication outside setup-AP mode.

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
- ArduinoOTA over the local network
- a factory image from a tagged GitHub release

ArduinoOTA starts without password authentication after a fresh installation or migration to schema 6. This allows an upload with an empty PlatformIO `--auth` value. A password can be added under **Sicherheit → ArduinoOTA**. Once configured, PlatformIO must send the same value through `GASMETER_OTA_PASSWORD`. The WebUI also provides an explicit option to remove the stored OTA password again.

Unauthenticated ArduinoOTA must only be used in a trusted, restricted management or IoT network. Do not expose TCP/UDP port 3232 to guest, public or Internet-facing networks.

A pending ESP-IDF OTA image is accepted only after the device has passed runtime health checks. Safe mode never accepts a pending image.

## Security

The dashboard status and Prometheus endpoint are read-only. The optional admin login protects configuration, logs, history export, manual polling, reset and restart outside setup-AP mode. With the default disabled setting, these functions are reachable by any host that can access the WebUI, so network isolation remains important. Passwords are never returned by `/api/config`; it reports only authentication state and whether an OTA password is configured. Configuration exports include secrets only when the admin login is enabled, the request is authenticated and `?secrets=EXPORT` is supplied.

The stored initial credentials `admin` / `admin` are intentionally predictable and are not suitable for permanent operation. Change them when enabling the admin login, and do not expose the HTTP WebUI directly to the Internet. See [SECURITY.md](SECURITY.md) for ArduinoOTA, TLS, Secure Boot and network-segmentation guidance.

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

A release is created from the **Release** workflow on `main`. Enter the semantic version once, without a `v` prefix:

1. Open **Actions → Release → Run workflow**.
2. Select `main` and enter a version such as `3.2.0`.
3. Run the workflow.

The workflow requires a version newer than the latest release, serializes release runs, tests and builds the firmware, verifies the embedded version, then creates the matching `v3.2.0` tag, release and artifact names. Normal development builds identify themselves as `dev+<commit>`. Protect the `v*` tag namespace against updates and deletions with a GitHub tag ruleset.

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
| `GET /api/config` | Redacted configuration and credential status | When admin login is enabled |
| `POST /api/config` | Validate and save | When admin login is enabled |
| `GET /api/config/export` | Export configuration | When admin login is enabled |
| `GET /api/usage.csv` | Completed daily history | When admin login is enabled |
| `POST /api/mbus/poll` | Trigger a poll | When admin login is enabled |
| `GET /api/logs` | Ring-buffer logs | When admin login is enabled |
| `POST /api/restart` | Planned restart | When admin login is enabled |
| `POST /api/factory-reset` | Reset with `{"confirm":"RESET"}` | When admin login is enabled; no setup bypass |

## License

See [LICENSE](LICENSE).
