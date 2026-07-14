# Security

## Threat model

This firmware is intended for a trusted home/IoT network. The WebUI uses HTTP Digest authentication and therefore must not be exposed directly to the public Internet. Put the ESP32 in a dedicated VLAN, restrict management access and use a VPN for remote administration.

## Defaults

- Setup AP SSID includes a device suffix.
- Setup AP password is randomly generated on every setup-AP start and printed only to serial.
- WebUI and OTA passwords are randomly generated, persisted in NVS and printed to serial during boot.
- Sensitive endpoints require authentication outside setup AP.
- Factory reset never allows the setup-AP authentication bypass and additionally requires the literal confirmation `RESET`.
- API configuration responses contain empty password/CA fields.
- Config export omits secrets by default. Secrets are included only with `?secrets=EXPORT` after authentication.
- OTA may optionally require an `X-MD5` integrity value; production authenticity should use Secure Boot V2.

## MQTT TLS

Enable `mqtt.tls` and provide a CA certificate in PEM format. `tls_insecure` exists only for temporary diagnostics and disables broker certificate verification. Do not use it permanently.

For a private Mosquitto CA, paste the CA certificate, not the broker private key. The broker hostname must match the certificate SAN/CN.

## Signed OTA / Secure Boot

The release workflow can create an app signed with ESP Secure Boot V2 when `ESP_SIGNING_KEY` is stored as an encrypted repository secret. No signing key belongs in the repository.

Secure Boot and Flash Encryption must be provisioned carefully on each physical ESP32. They can make recovery difficult or irreversible depending on eFuse settings. Follow the Espressif documentation for the exact chip revision and back up all required keys before burning eFuses.

A signed release alone is not sufficient: the device bootloader must be configured to verify the signature. Without Secure Boot, SHA-256 and MD5 files provide integrity checking but not authenticity.

## NVS credentials

Wi-Fi, MQTT, WebUI and OTA credentials are stored in NVS. For stronger at-rest protection, provision ESP32 Flash Encryption/NVS Encryption. The firmware does not pretend that ordinary Preferences storage is encrypted.

## Reporting

Do not publish Wi-Fi credentials, MQTT credentials, CA private keys, signed firmware keys or complete configuration exports in public issues. Report security problems privately to the repository owner.
