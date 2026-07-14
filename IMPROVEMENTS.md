# Improvement status

The v3 refactor implements the previously planned architecture, security, parser, CI, release and Home Assistant work. The current design and remaining limitations are documented in `ARCHITECTURE.md`, `SECURITY.md` and the issue tracker instead of maintaining a second aspirational feature list.

Hardware validation still matters: test each release against a real BK-G4 telegram, the specific M-Bus adapter and the target ESP32 board before production deployment.
