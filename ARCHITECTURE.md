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

`MBusReader` sends the BK-G4 request frame and reads bytes without blocking the main loop. It ignores request echoes, ACK bytes and line noise until a valid long-frame header is found, then resynchronizes if a malformed header contains a later `0x68` start byte. `MBusProtocol` determines the expected frame length as soon as the header is available and processes the response immediately rather than waiting for the timeout.

Validation order:

1. long-frame header and duplicated length
2. total frame length
3. stop byte
4. checksum over C/A/CI/data
5. BK-G4 volume record (`DIF 0x0C`, `VIF 0x13`)
6. every BCD nibble must be 0–9

The parser is independent from Arduino and covered by native tests for valid, checksum-invalid, BCD-invalid, stop-byte-invalid, length-invalid, unsupported and missing-volume-record frames.

## Configuration and migration

`CONFIG_SCHEMA_VERSION` is stored in the `gasmeter` NVS namespace. On first v3 boot, the firmware imports the previous keys from `gas-config`, validates them and persists the new schema. Secrets are generated once when missing.

Configuration JSON is assembled in a bounded per-request buffer, limited to 8 KiB, and then parsed with ArduinoJson. Changes are applied transactionally: validation failures restore the previous in-memory configuration, empty secret fields mean "unchanged", and a successful import is persisted before the API reports success.

A factory reset clears configuration, legacy configuration, usage baselines and the boot-guard state so no stale consumption or safe-mode data survives the reset.

## Consumption accounting

The corrected total is:

```text
corrected volume = meter reading + configured meter offset
```

Energy and variable tariff cost are accumulated from positive meter deltas. This keeps the published `total_increasing` energy value monotonic and prevents later changes to the calorific value, correction factor or tariff from repricing all historic consumption. Daily, monthly and yearly volume baselines and variable-cost accumulators are persisted at period changes and at a one-hour checkpoint, limiting flash writes.

A decreasing total is treated as a meter reset/replacement: period baselines and period costs restart, while cumulative energy does not decrease. Flow values outside the configured plausible maximum are rejected.

Tariffs are fixed-size, dated records. The latest record whose `valid_from` is not in the future is used for new consumption increments and the current fixed-cost estimate.

## Failure and OTA behavior

A boot-failure counter is incremented at startup and cleared after 60 seconds of stable operation. Three consecutive unstable boots start a setup-only safe mode. After the stable interval, the application calls `esp_ota_mark_app_valid_cancel_rollback()` so ESP-IDF rollback can retain the previous image when the new image fails early.
