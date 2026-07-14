# Architecture

## Runtime flow

```text
main.cpp
  ├─ Config                 NVS, migration, validation, secrets and tariffs
  ├─ WiFiManager            station/setup AP, static IPv4 and captive DNS
  ├─ MBusReader             non-blocking UART state machine
  │    └─ MBusProtocol      platform-independent frame parser
  ├─ UsageTracker           flow, meter replacement handling and period baselines
  │    └─ CoreLogic         platform-independent validation/calculation helpers
  ├─ MQTTHandler            transport, TLS, reconnect backoff, discovery and commands
  ├─ WebServerManager       Async WebUI/API, authentication, OTA and metrics
  └─ Logger                 bounded in-memory log ring
```

There is one source of truth for configuration (`Config`) and one implementation for each subsystem. The old global copies in `main.cpp` were removed.

## M-Bus handling

`MBusReader` sends the BK-G4 request frame and reads bytes without blocking the main loop. `MBusProtocol` determines the expected frame length as soon as the long-frame header is available and processes the response immediately rather than waiting for the timeout.

Validation order:

1. long-frame header and duplicated length
2. total frame length
3. stop byte
4. checksum over C/A/CI/data
5. BK-G4 volume record (`DIF 0x0C`, `VIF 0x13`)
6. every BCD nibble must be 0–9

The parser is independent from Arduino and covered by native tests with valid, checksum-invalid and BCD-invalid fixtures.

## Configuration and migration

`CONFIG_SCHEMA_VERSION` is stored in the `gasmeter` NVS namespace. On first v3 boot, the firmware imports the previous keys from `gas-config`, validates them and persists the new schema. Secrets are generated once when missing.

Configuration JSON is parsed through ArduinoJson's request handler. Request bodies are limited to 8 KiB and no shared static request buffer is used.

## Consumption accounting

The corrected total is:

```text
corrected volume = meter reading + configured meter offset
energy = corrected volume × calorific value × correction factor
```

Daily, monthly and yearly baselines are stored only when a period changes and at a one-hour checkpoint, limiting flash writes. A decreasing total is treated as a meter reset/replacement and period baselines are restarted. Flow values outside the configured plausible maximum are rejected.

Tariffs are fixed-size, dated records. The latest record whose `valid_from` is not in the future is used for cost calculation.

## Failure and OTA behavior

A boot-failure counter is incremented at startup and cleared after 60 seconds of stable operation. Three consecutive unstable boots start a setup-only safe mode. After the stable interval, the application calls `esp_ota_mark_app_valid_cancel_rollback()` so ESP-IDF rollback can retain the previous image when the new image fails early.
