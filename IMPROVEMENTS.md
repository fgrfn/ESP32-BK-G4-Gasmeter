# Improvement status

Firmware 3.1 implements the planned architecture, security, parser, time handling, unique MQTT topics, extended Home Assistant discovery, full configuration WebUI, history export, dependency monitoring and boot/OTA hardening.

Gas-cost forecasting and browser firmware upload are intentionally outside the firmware scope. Cost models belong in Home Assistant; supported firmware update paths are documented in `README.md` and `RELEASE.md`.

The remaining release gate is physical validation against a real BK-G4 telegram, the specific M-Bus adapter and the target ESP32 board. Add anonymized real telegram captures to `test/fixtures/` when available.
