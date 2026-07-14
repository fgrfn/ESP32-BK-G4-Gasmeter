# Changelog

## 3.1.1 - unreleased

### Changed

- Initial WebUI credentials are now the documented defaults `admin` / `admin`.
- WebUI username and password remain editable under **Configuration → Security** and are persisted in NVS.
- Configuration schema is now `5`; upgrading from an older schema resets the WebUI login once to the documented defaults.
- ArduinoOTA continues to use its own separately generated/configurable password.
- The embedded WebUI has been redesigned as a responsive dashboard with clearer status, consumption and maintenance sections.
- The current meter reading is displayed as a mechanical gas-meter counter with black whole-number rollers and red decimal rollers.

### Security

- The firmware logs a warning while the default WebUI credentials are active.
- Documentation now explicitly requires changing the initial login after commissioning.

## 3.1.0 - unreleased

### Added

- Explicit NTP synchronization state; period accounting never uses a fabricated fallback date.
- Reset-reason-aware boot guard with planned-restart markers and delayed OTA health acceptance.
- Unique default MQTT base topic `gasmeter/<device_id>` with migration from the old shared default.
- Home Assistant poll/restart buttons, measurement number entities and expanded health diagnostics.
- Retained-discovery cleanup for removed or disabled Home Assistant entities.
- Complete WebUI configuration for static IPv4, timezone, TLS, limits, security and flow warnings.
- JSON configuration import, completed-day CSV history, log viewer and M-Bus hex viewer in the WebUI.
- Continuous-flow plausibility warning exposed through WebUI, MQTT, Home Assistant and Prometheus.
- 31-day completed-day volume/energy history.
- Native DST/month/year boundary tests, wider M-Bus range tests, deterministic fuzzing and capture replay tooling.
- Weekly PlatformIO dependency audit and optional Renovate configuration.
- Hardware, commissioning and Home Assistant documentation.
- Compile-time GPIO overrides for alternative tested board layouts.

### Changed

- Cumulative energy and period energy are derived from positive meter deltas.
- Diagnostics are published periodically even without a new meter telegram.
- A pending OTA image is accepted only after network/time/runtime health checks.
- Firmware version is now `3.1.0`; configuration schema is now `4`.

### Removed

- Firmware tariff records and all gas-cost/cost-forecast calculations.
- Home Assistant cost entities and WebUI cost display.
- Non-functional browser firmware upload and `/api/ota` endpoint.
- Duplicate NVS save after successful WebUI configuration import.

### Fixed

- Planned configuration, WebUI, MQTT and ArduinoOTA restarts no longer count as unstable boots.
- Day/month/year baselines are no longer changed before NTP synchronization.
- Multiple devices no longer share the default MQTT state topics.
- Stale command entities are removed when MQTT commands are disabled.

## 3.0.0 - 2026-07-14 development baseline

### Breaking

- Replaced the duplicated legacy/module runtime with one modular implementation.
- MQTT topics use `<base>/state`, `<base>/diagnostics` and `<base>/availability` JSON payloads.
- Web configuration and management endpoints require authentication outside setup mode.

### Added

- Full M-Bus long-frame, checksum and BCD validation with native fixtures.
- Request-length limits and ArduinoJson-based Async API handlers.
- Random setup AP, WebUI and OTA credentials.
- MQTT TLS, exponential reconnect backoff, stable Home Assistant IDs and optional commands.
- Daily/monthly/yearly consumption, meter offset and replacement detection.
- Prometheus endpoint, safe mode and ESP-IDF OTA rollback support.
- Factory and OTA release images, checksums and optional Secure Boot V2 signing.
- Native tests, debug build, static analysis, size guard and Dependabot for Actions.
- Migration from the old `gas-config` NVS namespace.

### Fixed

- Accepted the valid default configuration on fresh installations.
- Made JSON configuration updates transactional and preserved secrets when empty fields are imported.
- Extended factory reset to remove usage baselines and boot-guard state.
- Resynchronized M-Bus input after request echoes, ACK bytes, malformed headers and line noise.
- Calculated average M-Bus response time from completed responses instead of all polls.
- Removed plaintext password responses from `/api/config`.
- Removed unauthenticated reset/config/OTA management.
- Removed shared static HTTP body buffers and manual JSON parsing.
- Fixed version drift by using `include/version.h` as the sole source and tag verification.
- Replaced archived `me-no-dev` Async dependencies with pinned `ESP32Async` releases.
