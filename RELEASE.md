# Release process

1. Run `pio test -e native`, `pio run -e esp32dev` and `pio check -e esp32dev --fail-on-defect high`.
2. Update `FIRMWARE_VERSION` in `include/version.h` and add the final date/notes to `CHANGELOG.md`.
3. Commit and merge the change.
4. Create and push an exact matching tag such as `v3.0.0`.
5. Verify the GitHub release contains OTA, factory and SHA-256 artifacts.

Use the OTA image only for an already provisioned device. Use the factory image at address `0x0` for a complete initial flash:

```bash
python -m esptool --chip esp32 --port /dev/ttyUSB0 write_flash 0x0 ESP32-BK-G4-Gasmeter-3.0.0-factory.bin
```

The optional signed OTA image is useful only on devices provisioned with the matching Secure Boot V2 public-key digest.
