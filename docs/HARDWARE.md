# Hardware and wiring

## Meter and interface

The target meter is the Honeywell/Elster **BK-G4AT (BK-G4 AT)** with an **Absolute ENCODER** index and a compatible M-Bus interface. It provides the full meter reading in a digital telegram. A Reed-switch pulse pickup relies on a pulse magnet in a pulse-capable index. Use the encoder interface on the BK-G4AT. [Honeywell Absolute ENCODER](https://process.honeywell.com/us/en/products/utilities/gas/gas-metering/smart-gas-meters/absolute-encoder-ae5) · [Honeywell BK-G4/BK-G4T datasheet](https://docuthek.kromschroeder.com/download.php?doc=59327&lang=de)

Identify the fitted interface before choosing the adapter. A two-wire **ACM M-BUS WIRE** module requires a suitable wired M-Bus master. For an adapter built for the encoder's communication-module socket, follow that adapter's wiring documentation. The pin table below describes the **3.3 V UART side of the adapter**. [Honeywell ACM interface overview](https://process.honeywell.com/us/en/products/utilities/gas/gas-metering/smart-gas-meters/absolute-encoder-ae5)

The firmware polls primary address `0x00` and expects an M-Bus long frame with an eight-digit BCD volume record (`DIF 0x0C`, `VIF 0x13`). Check the reply from your meter with the commissioning steps below. Module labels such as SCR and Wireless M-Bus identify different communication interfaces.

## Electrical warning

The ESP32 UART must **not** be connected directly to the two-wire M-Bus line. M-Bus uses different voltage and current levels. Use a suitable M-Bus master/interface module that converts the bus to 3.3 V UART levels and provides the isolation required by the installation.

This community project is not a certified gas-safety device. Do not use it to replace an approved gas detector, shutoff system or meter supplied by the utility.

## Default ESP32 pins

| Function | Default pin | Interface side |
|---|---:|---|
| UART TX | GPIO17 | M-Bus adapter RX |
| UART RX | GPIO16 | M-Bus adapter TX |
| Status LED | GPIO2 | onboard LED on many ESP32 boards |
| Factory-reset button | GPIO0 | BOOT button on many ESP32 boards |
| Ground | GND | adapter GND, when required by the adapter |
| Supply | adapter-specific | follow the adapter documentation |

UART settings are **2400 baud, 8 data bits, even parity, 1 stop bit (8E1)**.

## Compile-time pin overrides

Pins can be changed without editing source files by adding build flags to a PlatformIO environment:

```ini
build_flags =
    ${common.build_flags}
    -D GAS_MBUS_RX_PIN=18
    -D GAS_MBUS_TX_PIN=19
    -D GAS_STATUS_LED_PIN=2
    -D GAS_RESET_BUTTON_PIN=0
```

Create a separate environment for every tested board/pin combination. Keep the production `esp32dev` environment unchanged until the alternative hardware has passed a real telegram test.

## Commissioning checklist

1. Confirm the adapter output is safe for 3.3 V ESP32 GPIOs.
2. Confirm TX and RX are crossed between ESP32 and adapter.
3. Confirm UART is configured for 2400/8E1.
4. Open the serial monitor and verify that a poll receives a long frame beginning with `68` and ending with `16`.
5. Export the last hex telegram from the WebUI diagnostics page.
6. Replay the capture on a computer:

```bash
python tools/replay_mbus.py capture.hex
```

7. Compare the decoded value with the mechanical meter display.
8. Test at least one reboot, one Wi-Fi outage and one MQTT outage before relying on the data.

## Capturing a telegram for tests

Store one telegram per line as hexadecimal bytes. Comments after `#` are allowed:

```text
68 09 09 68 08 00 72 0C 13 56 34 12 00 35 16 # 123.456 m3
```

Before committing a real capture, remove serial numbers or other meter-specific identifiers that are not needed by the parser test.
