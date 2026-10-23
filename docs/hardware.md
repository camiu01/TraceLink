# Hardware and wiring

This document separates what the firmware expects from what has been physically verified. Paths are relative to the repository root.

## Supported target

- Board family: classic ESP32 DevKit with ESP32-WROOM-32 (`board = esp32dev`, platform `espressif32@6.10.0`).
- Verified specimen: ESP32-D0WD-V3 rev 3.1 (dual core, 240 MHz, 4 MB flash), reached through a Silicon Labs CP210x USB-UART bridge. Firmware build: RAM 20.0%, flash 62.5% of the default partition (~819 kB of 1310720 bytes).
- GPIO16/17 assignments apply to WROOM-32; other variants (WROVER, S3, C3, …) are not supported by this pin map.

## Pin map (firmware, `include/config.h`)

| ESP32 GPIO | Function | Note |
| --- | --- | --- |
| GPIO1 / GPIO3 | UART0: USB flashing and 115200 baud monitor | Must stay free |
| GPIO16 / RX2 | RS232 UART2 RX | From module TXD |
| GPIO17 / TX2 | RS232 UART2 TX | To module RXD; only driven after arming |
| GPIO26 | RS485 UART1 RX | From module TX |
| GPIO27 | RS485 UART1 TX | To module RX; only driven after arming |
| GPIO25 | RS485 DE//RE | Driven LOW except during armed sends; leave unconnected on auto-direction modules |
| GPIO32 | CAN TX | To transceiver TXD |
| GPIO33 | CAN RX | From transceiver RXD |

Initial speeds: RS232 9600 8N1, RS485 9600 8N1, CAN 500 kbit/s listen-only.

## Modules on hand (user-provided hardware)

| Module | Type | Project use |
| --- | --- | --- |
| SP3232-class TTL/RS232 module (blue, Sipex chip) | Level converter, 3.3–5 V capable | Powered at 3.3 V; RS232-side RX listens to the observed line, RS232-side TX left unconnected for passive sniffing |
| Isolated auto-direction TTL/RS485 module | Level converter with galvanic isolation | Powered at 3.3 V; A+/B+ pair, earth terminal floating |
| SN65HVD230-class 3.3 V CAN transceiver | Proposed, not yet purchased or connected | Physical layer for the TWAI controller |

A B0505XT-1WR3 DC/DC converter is not required: the RS485 module already integrates isolation. It is a power converter, not a signal isolator or transceiver.

## Electrical boundaries

- ESP32 GPIOs are 3.3 V only and are not 5 V tolerant. RS232, RS485, and CAN signals must never reach a GPIO directly.
- Power every TTL side at 3.3 V. Do not feed 12 V (or any bus-side supply) into a module's TTL side or into the ESP32.
- Joining signal grounds between the monitored equipment and the ESP32 also joins whatever the board's supply is referenced to (a PC over USB, or a mains adapter). Confirm ground compatibility first; if it cannot be confirmed, use galvanically isolated receivers. The isolated RS485 module covers its own bus side only.
- Passive RS232 sniffing taps one direction in parallel with the existing line: device TX line → module RS232-side RX, shared signal ground after compatibility is confirmed, module RS232-side TX unconnected. Watching both directions needs a second receiver and a firmware extension (planned work, not implemented).

## Physical validation status

- Firmware upload and boot verified on the ESP32-D0WD-V3 (October 2026): receive-only banner and AP announcement observed on the serial monitor. TX was not armed.
- RS232 capture from a real device was reported working by the user; not independently re-verified.
- RS485 and CAN have not been exercised on hardware. CAN needs a bench bus with at least two active nodes and proper termination; in normal mode the node ACKs traffic even without user sends.
- No electrical measurements (levels, currents, termination) have been recorded in this project.
