# Firmware 3.1 implementation summary

This document records the scope of the 3.1 update for reviewers and future maintainers.

## Reliability

- NTP synchronization is explicit; no fabricated calendar timestamp is used.
- Period totals are updated only after time synchronization.
- Flow is calculated across the interval between actual meter increments.
- Panic/watchdog/brownout resets are distinguished from planned restarts.
- Pending OTA images are accepted only after runtime health checks.

## Removed scope

- Tariff and gas-cost forecasting
- Cost entities in MQTT/Home Assistant
- Browser firmware upload and `/api/ota`

## Added scope

- Unique default MQTT topic per device
- Expanded Home Assistant discovery and retained cleanup
- Full configuration WebUI
- 31-day daily history and CSV export
- Continuous-flow plausibility warning
- Extended Prometheus metrics
- ESP32-S3 build target and compile-time pin overrides
- DST/month/year tests, parser fuzzing and telegram replay
- Weekly dependency audit

Physical validation with the actual BK-G4, M-Bus adapter and target board remains mandatory before tagging the release.
