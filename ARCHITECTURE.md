# Architecture

## Runtime flow

```text
main.cpp
  ├─ Config                 NVS, migration, validation and secrets
  ├─ BootGuard              reset-reason tracking, safe mode and OTA validation
  ├─ TimeManager            NTP synchronization state
  ├─ WiFiManager            station/setup AP, static IPv4 and captive DNS
  ├─ MBusReader             non-blocking UART state machine and health state
  │    └─ MBusProtocol      platform-independent frame parser
  ├─ UsageTracker           energy deltas, periods, history and flow warning
  │    ├─ CoreLogic         platform-independent validation/calculation helpers
  │    └─ UsageLogic        platform-independent local period-key generation
  ├─ MQTTHandler            TLS, reconnect backoff, discovery and commands
  ├─ WebServerManager       WebUI/API, authentication, CSV and metrics
  └─ Logger                 bounded in-memory log ring
```

There is one source of truth for configuration (`Config`) and one implementation for each subsystem.

## M-Bus handling

`MBusReader` sends the BK-G4 request frame and reads bytes without blocking the main loop. It ignores request echoes, ACK bytes and line noise until a valid long-frame header is found, then resynchronizes if a malformed header contains a later `0x68` start byte.

Validation order:

1. long-frame header and duplicated length
2. total frame length
3. stop byte
4. checksum over C/A/CI/data
5. BK-G4 volume record (`DIF 0x0C`, `VIF 0x13`)
6. every BCD nibble must be 0–9

The parser is independent from Arduino and covered by valid/range/error fixtures, deterministic random-input fuzzing and a host-side capture replay tool.

## Time and period accounting

The meter total and cumulative energy can be updated before NTP is available, but day/month/year periods are changed only when `TimeManager` reports a synchronized Unix timestamp. The firmware never substitutes a fabricated calendar date.

`UsageLogic` creates local day, month and year keys using the configured POSIX timezone. Native tests cover DST transitions and month/year boundaries.

The corrected total and energy increment are:

```text
corrected volume = meter reading + configured meter offset
energy increment = positive volume delta × calorific value × correction factor
```

Cumulative energy is monotonic. Changes to calorific value or correction factor affect only later increments. A decreasing meter total is treated as replacement/reset and does not reduce cumulative energy.

Daily, monthly and yearly volume/energy baselines are persisted. Completed days are retained as a 31-record ring and exported as CSV. The firmware intentionally contains no tariff or gas-cost model.

## Continuous-flow warning

The warning uses calculated flow plus a grace window for meter resolution and polling gaps. It is a plausibility indicator only and is exposed through the WebUI, MQTT, Home Assistant and Prometheus.

## Configuration and migration

`CONFIG_SCHEMA_VERSION` is stored in the `gasmeter` NVS namespace. Schema 4:

- removes firmware tariff/cost configuration
- migrates the former default MQTT base topic `gasmeter` to `gasmeter/<device_id>`
- preserves explicitly configured custom MQTT topics
- adds continuous-flow warning settings

Configuration JSON is assembled in a bounded per-request buffer, limited to 8 KiB, and parsed with ArduinoJson. Changes are transactional: validation failures restore the previous in-memory configuration, empty secret fields mean "unchanged", and a successful import is persisted before the API reports success.

Schema 7 adds the persistent `security.web_auth_enabled` switch. It is disabled for fresh installations and factory resets. Existing schema 1-6 installations retain their prior authenticated behavior on upgrade.

A factory reset clears configuration, legacy configuration, usage/history and boot-guard state.

## MQTT and Home Assistant

State, diagnostics and availability use a device-specific base topic by default. Discovery IDs and object IDs also contain the device ID. The firmware clears retained discovery for removed cost entities and for command entities when commands are disabled.

Diagnostics are published periodically even when no new meter telegram arrives, so uptime, time synchronization and health entities remain current.

## Boot and OTA behavior

`BootGuard` counts only panic, watchdog and brownout reset reasons as unstable starts. Planned configuration, WebUI, MQTT and ArduinoOTA restarts are marked in NVS and do not increase the failure counter.

Three unstable resets enable safe mode. A pending ESP-IDF OTA image is accepted only when:

- safe mode is not active
- the minimum stable-runtime interval has elapsed
- Wi-Fi is connected
- NTP is synchronized
- M-Bus has produced a healthy response, or the longer fallback interval has elapsed

The browser firmware-upload route was removed. Serial/USB flashing, ArduinoOTA and tagged release images remain supported.
