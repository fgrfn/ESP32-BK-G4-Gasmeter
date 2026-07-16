# Security

## Threat model

This firmware is intended for a trusted home/IoT network. Optional HTTP Digest authentication can protect WebUI administration, but it is disabled by default and the WebUI must not be exposed directly to the public Internet. Put the ESP32 in a dedicated VLAN, restrict management access and use a VPN for remote administration.

## Defaults

- Setup AP SSID includes a device suffix.
- Setup AP password is randomly generated on every setup-AP start and printed only to serial.
- WebUI admin authentication is disabled after a fresh installation or factory reset.
- Authentication can be enabled under **Sicherheit**; its initial credentials are `admin` / `admin` and must be changed when enabling it.
- The authentication setting, username and password are persisted in NVS and survive normal restarts and OTA updates.
- ArduinoOTA is enabled after Wi-Fi connects and has no password after a fresh installation, factory reset or migration to configuration schema 6.
- An optional ArduinoOTA password can be added or removed under **Sicherheit → ArduinoOTA**.
- Sensitive HTTP endpoints require authentication outside setup AP when the optional admin login is enabled.
- Factory reset does not use the setup-AP bypass when authentication is enabled and additionally requires the literal confirmation `RESET`.
- API configuration responses contain empty password/CA fields and expose only whether an OTA password is configured.
- Config export omits secrets by default. Secrets are included only with `?secrets=EXPORT` after authentication.
- The browser firmware-upload endpoint is intentionally not present.

With the admin login disabled, every host that can reach the WebUI can change configuration, view logs and trigger maintenance actions. The predictable initial WebUI credentials and unauthenticated ArduinoOTA default are intended only to simplify local commissioning. Enable the login with a unique password or strictly limit network access. Do not expose the device to the Internet or permit access to TCP/UDP port 3232 from guest, public or untrusted VLANs.

## MQTT TLS

Enable `mqtt.tls` and provide a CA certificate in PEM format. `tls_insecure` exists only for temporary diagnostics and disables broker certificate verification. Do not use it permanently.

For a private Mosquitto CA, paste the CA certificate, not the broker private key. The broker hostname must match the certificate SAN/CN.

MQTT command entities can restart the device and change measurement settings. Enable them only when broker access is restricted appropriately.

## ArduinoOTA

ArduinoOTA is available only after the device connects to Wi-Fi. With an empty OTA password, every host that can reach the ArduinoOTA service can attempt a firmware upload. Network isolation is therefore mandatory when password authentication is disabled.

When an OTA password is configured, PlatformIO must provide the same value through the `--auth` option. Password authentication reduces accidental or unauthorized uploads but does not replace network isolation or signed firmware verification.

Recommended controls:

- place the ESP32 in a dedicated IoT or management VLAN
- permit ArduinoOTA traffic only from trusted administration devices
- block port 3232 between guest/untrusted networks and the ESP32
- use a VPN for remote management
- configure an OTA password when multiple users or devices can access the VLAN

## Signed OTA / Secure Boot

The release workflow can create an app signed with ESP Secure Boot V2 when `ESP_SIGNING_KEY` is stored as an encrypted repository secret. No signing key belongs in the repository.

Secure Boot and Flash Encryption must be provisioned carefully on each physical ESP32. They can make recovery difficult or irreversible depending on eFuse settings. Follow the Espressif documentation for the exact chip revision and back up all required keys before burning eFuses.

A signed release alone is not sufficient: the device bootloader must be configured to verify the signature. Without Secure Boot, SHA-256 files provide integrity checking but not authenticity.

A pending ESP-IDF OTA image is accepted only after runtime health checks. Safe mode never marks a pending image valid.

## NVS credentials

Wi-Fi, MQTT, WebUI and an optional ArduinoOTA password are stored in NVS. For stronger at-rest protection, provision ESP32 Flash Encryption/NVS Encryption. Ordinary Preferences storage is not encrypted by this firmware.

## Reporting

Do not publish Wi-Fi credentials, MQTT credentials, CA private keys, signed firmware keys or complete configuration exports in public issues. Report security problems privately to the repository owner.
