# Changelog

## 3.0.0 - unreleased

### Breaking

- Replaced the duplicated legacy/module runtime with one modular implementation.
- MQTT topics now use `<base>/state`, `<base>/diagnostics` and `<base>/availability` JSON payloads.
- Web configuration and management endpoints require authentication outside setup mode.

### Added

- Full M-Bus long-frame, checksum and BCD validation with native fixtures.
- Request-length limits and ArduinoJson-based Async API handlers.
- Random setup AP, WebUI and OTA credentials.
- MQTT TLS, exponential reconnect backoff, stable Home Assistant IDs and optional commands.
- Daily/monthly/yearly consumption, dated tariffs, costs, meter offset and replacement detection.
- Prometheus endpoint, safe mode and ESP-IDF OTA rollback acceptance.
- Factory and OTA release images, checksums and optional Secure Boot V2 signing.
- Native tests, debug build, static analysis, size guard and Dependabot for Actions.
- Migration from the old `gas-config` NVS namespace.

### Fixed

- Accepted the valid default tariff date `1970-01-01` on fresh installations.
- Made JSON configuration updates transactional and preserved secrets when empty fields are imported.
- Extended factory reset to remove usage baselines and boot-guard state.
- Preserved cumulative energy and historic variable tariff costs when conversion factors or prices change.
- Resynchronized M-Bus input after request echoes, ACK bytes, malformed headers and line noise.
- Calculated average M-Bus response time from completed responses instead of all polls.
- Removed plaintext password responses from `/api/config`.
- Removed unauthenticated reset/config/OTA management.
- Removed shared static HTTP body buffers and manual JSON parsing.
- Fixed version drift by using `include/version.h` as the sole source and tag verification.
- Replaced archived `me-no-dev` Async dependencies with pinned `ESP32Async` releases.
- Corrected flow and cost calculations that previously extrapolated one instantaneous flow value.
