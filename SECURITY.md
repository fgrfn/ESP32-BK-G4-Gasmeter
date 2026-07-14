# Security

## Threat model

This firmware is intended for a trusted home/IoT network. The WebUI uses HTTP Digest authentication and therefore must not be exposed directly to the public Internet. Put the ESP32 in a dedicated VLAN, restrict management access and use a VPN for remote administration.

## Defaults

- Setup AP SSID includes a device suffix.
- Setup AP password is randomly generated on every setup-AP start and printed only to serial.
- Initial WebUI credentials are `admin` / `admin` and must be changed under **Configuration → Security** after commissioning.
- WebUI username and password changes are persisted in NVS and survive normal restarts and OTA updates.
- ArduinoOTA uses a separately generated password that is persisted in NVS and printed to serial during boot.
- Sensitive endpoints require authentication outside setup AP.
- Factory reset never allows the setup-AP authentication bypass and additionally requires the literal confirmation `RESET`.
- API configuration responses contain empty password/CA fields.
- Config export omits secrets by default. Secrets are included only with `?secrets=EXPORT` after authentication.
- The browser firmware-upload endpoint is intentionally not present.

The predictable initial WebUI credentials are intended only to simplify local commissioning. Do not leave them unchanged, expose the WebUI to the Internet or permit access from untrusted VLANs.

## MQTT TLS

Enable `mqtt.tls` and provide a CA certificate in PEM format. `tls_insecure` exists only for temporary diagnostics and disables broker certificate verification. Do not use it permanently.

For a private Mosquitto CA, paste the CA certificate, not the broker private key. The broker hostname must match the certificate SAN/CN.

MQTT command entities can restart the device and change measurement settings. Enable them only when broker access is restricted appropriately.

## ArduinoOTA

ArduinoOTA is available only after the device connects to Wi-Fi and requires the configured OTA password. ArduinoOTA password authentication does not replace network isolation or signed firmware verification. Prefer a management VLAN/VPN and do not expose ArduinoOTA ports to untrusted networks.

## Signed OTA / Secure Boot

The release workflow can create an app signed with ESP Secure Boot V2 when `ESP_SIGNING_KEY` is stored as an encrypted repository secret. No signing key belongs in the repository.

Secure Boot and Flash Encryption must be provisioned carefully on each physical ESP32. They can make recovery difficult or irreversible depending on eFuse settings. Follow the Espressif documentation for the exact chip revision and back up all required keys before burning eFuses.

A signed release alone is not sufficient: the device bootloader must be configured to verify the signature. Without Secure Boot, SHA-256 files provide integrity checking but not authenticity.

A pending ESP-IDF OTA image is accepted only after runtime health checks. Safe mode never marks a pending image valid.

## NVS credentials

Wi-Fi, MQTT, WebUI and ArduinoOTA credentials are stored in NVS. For stronger at-rest protection, provision ESP32 Flash Encryption/NVS Encryption. Ordinary Preferences storage is not encrypted by this firmware.

## Reporting

Do not publish Wi-Fi credentials, MQTT credentials, CA private keys, signed firmware keys or complete configuration exports in public issues. Report security problems privately to the repository owner.