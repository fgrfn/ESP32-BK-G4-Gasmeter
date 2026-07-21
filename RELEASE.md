# Release process

1. Run `pio test -e native`, `pio run -e esp32dev`, the capture replay test and `pio check -e esp32dev --fail-on-defect high`.
2. Flash the candidate to the target ESP32 and verify a real BK-G4 telegram, NTP synchronization, MQTT discovery, reboot handling and CSV export.
3. Add the final date/notes to `CHANGELOG.md`. The firmware version is supplied by the release workflow and must not be edited in source files.
4. Commit and merge the change.
5. On the `main` branch, run the **Release** workflow and enter an exact semantic version without a `v` prefix, such as `3.2.0`.
6. Verify the GitHub release contains OTA, factory and SHA-256 artifacts.

The workflow validates that the version is newer than the latest release, prevents concurrent releases, runs the tests, injects the same version into the firmware, verifies the compiled binary and derives the tag, release name and artifact names from that one input. Configure a GitHub tag ruleset for `v*` that blocks tag updates and deletions so released versions cannot be moved later.

Normal local and pull-request builds are labelled `dev+<commit>`. To reproduce a release-labelled build locally, set `GASMETER_VERSION` explicitly:

```bash
GASMETER_VERSION=3.2.0 pio run -e esp32dev
```

Use the OTA image only with ArduinoOTA or another compatible external OTA process. The browser firmware-upload endpoint is not supported.

Use the factory image at address `0x0` for a complete initial flash:

```bash
python -m esptool --chip esp32 --port /dev/ttyUSB0 write_flash 0x0 ESP32-BK-G4-Gasmeter-3.2.0-factory.bin
```

After an OTA boot, confirm that `/api/status` reports:

- `safe_mode: false`
- `time_synchronized: true`
- `mbus.healthy: true`
- `ota_pending_validation: false` after the health-validation window

The optional signed OTA image is useful only on devices provisioned with the matching Secure Boot V2 public-key digest.
