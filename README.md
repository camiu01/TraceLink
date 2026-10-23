# TraceLink

TraceLink is a multi-bus monitor firmware for the classic ESP32 (ESP32-WROOM-32) that listens to RS232, RS485, and classic CAN at the same time and shows the captured traffic on a USB serial log and on a self-hosted web dashboard. It is built with PlatformIO and the Arduino framework.

It is a bench tool for people who need to observe traffic between industrial or embedded devices, estimate unknown line speeds, and occasionally send a command by hand. Transmission is disabled at boot and always requires explicit arming; the board starts in receive-only mode.

Current scope: one RS232 direction on UART2, one RS485 port on UART1, and one classic CAN bus on the built-in TWAI controller. History lives in RAM (128 records) and nothing is persisted across reboots.

[What it does](#what-it-does) · [Hardware](#hardware) · [Wiring](#wiring-and-board-configuration) · [Build and flash](#build-and-flash) · [Everyday use](#controls-and-everyday-use) · [Configuration](#configuration) · [Architecture](#firmware-architecture) · [Limitations](#limitations-and-troubleshooting) · [Development](#development) · [License](#credits-and-license)

## See it in action

Real boot output captured from the serial monitor (115200 baud) after flashing an ESP32-D0WD-V3 over USB on 22 October 2026:

```text
rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
...
TraceLink | RX only al boot | TX da abilitare esplicitamente
Wi-Fi: TraceLink-ESP32 | http://192.168.4.1
```

This shows the firmware booting, starting in receive-only mode, and announcing its Wi-Fi access point. Firmware status messages are currently in Italian. No dashboard screenshots or device photographs exist in the repository yet.

## What it does

| Feature | Observable behavior |
| --- | --- |
| Three-bus capture | RS232 (UART2), RS485 (UART1), and classic CAN (TWAI) traffic is collected by one acquisition task into a shared history with sequence numbers, timestamps, and per-bus counters. |
| Offline web dashboard | The ESP32 serves a page at `http://192.168.4.1` on its own access point, with no Internet and no external web dependencies. Shows live records, counters, loss warnings, filters, pause, autoscroll, and CSV export. The interface is bilingual (Italian/English) with a header toggle; the choice persists in the browser and the default follows the browser language. Status messages coming from the firmware remain in Italian. |
| USB serial mirror | Every captured record is also printed on the USB monitor at 115200 baud as `#seq ms SOURCE RX/TX` plus HEX data, with CAN id/flags/DLC. |
| Speed probing | On request, the firmware listens for a few seconds: serial baud is estimated from RX pulse timing (an estimate that requires manual confirmation, never auto-applied), and CAN is searched listen-only over 500/250/125/1000 kbit/s. |
| Manually armed transmission | Sending from the dashboard requires explicit arming, auto-disarms after 60 s of inactivity, fails closed on partial errors, and never resends automatically after an error. CAN sending is single-shot. |
| Device profiles | `web/systems.json` presets fill the send form and speed fields; loading a profile never arms TX and never sends. |
| Loss accounting | History overwrites, CAN missed/overrun/error counters, and browser-side gap counts are surfaced on the dashboard and in the API. |

### Available in the source

All features above are implemented in the current source tree. Implementation does not by itself establish correctness on every device or physical validation: see verification status below.

### Experimental and incompletely validated

- Serial baud estimation is a pulse-timing heuristic. It can pick the wrong rate depending on data patterns and it requires at least ~20 edges of real traffic. It never changes the configured speed by itself.
- Real-bus capture was reported working by the user on RS232; RS485 and CAN have not been exercised on hardware. Wi-Fi authentication and full dashboard behavior have not been independently verified.

### Planned or deferred

Open work is tracked in [TODO.md](TODO.md): web page verification on simulated APIs, safety and modularity audit, splitting `src/TraceLink.cpp` (currently over the project's 400-line file limit), replacing the example profile with documented device commands, and adding a license file.

## Hardware

| Component | Verified model or specification | Required / optional | Function |
| --- | --- | --- | --- |
| MCU board | ESP32-WROOM-32 DevKit; flashed and identified as ESP32-D0WD-V3 rev 3.1 | Required | Runs the firmware; USB port used for flashing and the monitor |
| RS232 converter | SP3232-class TTL/RS232 module, powered at 3.3 V | Optional (needed for RS232) | Level conversion between RS232 lines and ESP32 GPIOs |
| RS485 converter | Isolated auto-direction TTL/RS485 module, powered at 3.3 V | Optional (needed for RS485) | Level conversion and isolation for the RS485 pair |
| CAN transceiver | SN65HVD230-class 3.3 V module | Optional (needed for CAN) | Physical layer for the built-in TWAI controller |
| USB data cable | Connected to the board UART0 bridge (observed as a Silicon Labs CP210x on the host) | Required | Flashing and 115200 baud monitor |

ESP32 GPIOs are 3.3 V only and are not 5 V tolerant. Never connect RS232, RS485, or CAN signals directly to GPIOs; always go through the transceiver modules.

## Wiring and board configuration

The firmware expects this fixed map (`include/config.h`). UART0 (GPIO1/GPIO3) must stay free for USB flashing and the monitor.

| ESP32 | RS232 module (TTL side) | RS485 module (TTL side) | CAN module |
| --- | --- | --- | --- |
| 3V3 | VCC | VIN | VCC |
| GND | GND | GND (TTL side only) | GND |
| GPIO16 / RX2 | Module TXD | — | — |
| GPIO17 / TX2 | Module RXD | — | — |
| GPIO26 / UART1 RX | — | Module TX | — |
| GPIO27 / UART1 TX | — | Module RX | — |
| GPIO25 | — | Leave unconnected (auto-direction module) | — |
| GPIO32 | — | — | Transceiver TXD |
| GPIO33 | — | — | Transceiver RXD |

External bus-side notes, verified against the modules on hand:

- RS232: connect only the device line to be observed to the module RXD input, in parallel with the existing wiring; leave the module's RS232-side TX output unconnected for passive listening. RS232 levels must never reach a GPIO.
- RS485: A/B twisted pair; the earth terminal stays floating; the module already integrates isolation, so no extra DC/DC converter is needed.
- CAN: attach CANH/CANL on a bus with at least two active nodes and correct termination.

For first power-up after flashing, prefer a board connected via USB only, with bus wiring detached. Electrical compatibility between the monitored equipment ground and the board ground must be confirmed before joining them; when in doubt, use isolated receivers.

## Build and flash

Prerequisites: PlatformIO Core (verified with 6.1.19), Python 3, a data USB cable, and a host that sees the board's serial port.

1. Clone the repository and enter it:

   ```sh
   git clone https://github.com/camiu01/TraceLink.git
   cd TraceLink
   ```

2. (Recommended) Override the public fallback Wi-Fi password by creating `include/local_config.h` (git-ignored):

   ```c
   #pragma once
   #define TRACELINK_AP_PASSWORD "your-own-password"   // 8 to 63 characters
   ```

3. Build (this also regenerates the embedded web assets via `scripts/embed_web.py`):

   ```sh
   pio run
   ```

4. Identify the board's serial port (Windows example: Device Manager; the board appeared as `Silicon Labs CP210x USB to UART Bridge`). Do not flash to an automatically selected port without identifying it first.

5. Flash — this overwrites the firmware on the board:

   ```sh
   pio run --target upload --upload-port COM7   # replace COM7 with your port
   ```

6. Open the monitor:

   ```sh
   pio device monitor --baud 115200
   ```

7. First-use check: the boot log shows the receive-only banner and the access point line. Then connect a device to the `TraceLink-ESP32` network, keep the connection when the OS warns there is no Internet, and open `http://192.168.4.1`.

## Controls and everyday use

Main workflow: flash and boot → connect to the `TraceLink-ESP32` access point → open `http://192.168.4.1` → watch captured traffic appear in the table → optionally probe speeds → optionally arm TX and send one command at a time.

| Action | Result |
| --- | --- |
| Wait for traffic | Records appear in the table with sequence, ESP32 timestamp, bus/direction, HEX, and ASCII preview; per-bus counters update on the cards. Autoscroll follows new records and stops when you scroll up. |
| Pick an interface filter | Only records from that bus are shown. |
| Pausa vista / clear | Freezes or clears the browser view; capture on the device continues. |
| Esporta CSV | Downloads a `;`-separated CSV of the visible records with spreadsheet-formula neutralization. |
| Controlla velocità | Starts a passive listen (~3 s serial sampling plus CAN search). Serial results are shown as estimates to confirm by hand; a detected CAN rate is applied. Nothing is detected on a silent line. |
| Applica (speed selector on a card) | Changes that bus speed until the next reboot; not persistent. |
| Abilita trasmissione | Arms TX after a confirmation dialog. CAN switches to normal mode and sends ACKs on the bus. Auto-disarms 60 s after the last send. |
| Load a profile, fill the form, Invia una volta | Sends exactly one command (HEX or text on serial; id/flags/DLC on CAN), provided TX is armed. A queued CAN frame is not proof of delivery. |

Minimal end-to-end example (illustrative): on the dashboard select the RS232 card speed `9600`, arm TX, choose HEX, enter `01 03 00 00 00 02 C4 0B`, press **Invia una volta**. The send appears as a `TX` row. Whether the device answers depends entirely on its protocol; TraceLink does not compute checksums or decode application protocols.

Disconnected state: if the ESP32 is unreachable, the dashboard shows a connection error and retries automatically.

## Configuration

There is no runtime settings store. Behavior is configured at compile time and overridden locally; the web page can only change bus speeds until the next reboot.

### Compile-time configuration

All options live in `include/config.h`; changing them requires a rebuild and reflash.

| Option | Available configuration |
| --- | --- |
| `config::apSsid` | Access point name, default `TraceLink-ESP32`. |
| `TRACELINK_AP_PASSWORD` | AP password, 8–63 characters. Fallback is a public placeholder defined in `config.h`; override it in `include/local_config.h` (excluded from Git). A `static_assert` rejects out-of-range lengths at build time. |
| `config::rs232Baud`, `config::rs485Baud` | Initial serial speeds, default 9600 baud, format fixed to 8N1 (`config::serialFormat`). |
| `config::canBitrate` | Initial CAN speed, default 500 kbit/s; one of 125000, 250000, 500000, 1000000. |
| `config::rs232Rx/rs232Tx`, `rs485Rx/rs485Tx/rs485De`, `canTx/canRx` | GPIO map; see the wiring table. GPIO25 (DE/RE) is for non-auto-direction RS485 modules and stays LOW outside armed sends. |
| `config::historyCapacity` | 128 records in RAM. |
| `config::apiBatchSize` | 32 events per `/api/events` response. |
| `config::uartRxBufferSize` | 2048 bytes per UART. |
| `config::transmitWindowMs` | 60000 ms idle auto-disarm for TX. |

### Runtime configuration (dashboard)

Bus speeds can be changed per card and apply immediately until reboot. Values outside the supported rate tables are rejected by the firmware. Nothing else is configurable at runtime.

## Firmware architecture

Paths are relative to the repository root.

| Module | Responsibility |
| --- | --- |
| `include/config.h` | Compile-time settings and GPIO map; optional `local_config.h` override. |
| `include/capture.h` | `Capture` record, `Source` names, `CaptureHistory` ring buffer, HEX formatting. |
| `include/commands.h` | Strict HEX parser, overflow-checked number parser, supported rate tables, baud estimation from pulse histograms. Hardware-independent and host-testable. |
| `include/bus_interface.h` | `BusInterface` adapter contract plus `UartBus` and `CanBus` declarations. |
| `src/bus_interface.cpp` | UART/RS485 driver (optional DE/RE) and TWAI CAN driver (listen-only vs normal mode, single-shot TX, health counters). |
| `src/TraceLink.cpp` | FreeRTOS tasks, adapter registry, speed-scan state machine, TX arm/timeout logic, HTTP API. |
| `web/index.html` | Offline dashboard: polling, cards, filters, send form, CSV export. Bus bytes are rendered as text only. |
| `web/systems.json` | Device/command profiles, validated at build time. |
| `scripts/embed_web.py` | PlatformIO pre-script that packs the dashboard and profiles into a generated `web_page.h`; no filesystem upload is needed. |
| `test/native/history_test.cpp` | Host-side unit tests, no board required. |

Execution flow: `setup()` starts serial, the three bus adapters, the Wi-Fi access point, and the HTTP server, then creates two FreeRTOS tasks. The acquisition task (priority 2) owns the bus adapters exclusively — it polls each bus, pushes records into the shared history, advances the speed scan, and executes queued commands. The USB task (priority 1) drains the same history to the serial monitor. The Arduino `loop()` only serves HTTP clients. A single mutex protects the history and the status block; HTTP handlers validate requests and enqueue them for the acquisition task, they never touch the buses directly. Pulse sampling for baud estimation runs in a GPIO interrupt handler that only fills histogram bins.

Details: [docs/architecture.md](docs/architecture.md) · HTTP API: [docs/api.md](docs/api.md)

## Limitations and troubleshooting

| Symptom / limitation | Checks and boundary |
| --- | --- |
| Speed scan finds nothing | Detection needs traffic on the line. A silent line cannot be identified. Serial result is a timing estimate, not a guaranteed baud/parity/protocol match (format is fixed 8N1, no parity scan). |
| Records or CAN frames lost | History holds only 128 records in RAM and overwrites the oldest; the dashboard reports overwrites and browser-side gaps. CAN missed/overrun/error counters come from the TWAI driver. UART overflows are not counted. Speed changes, scanning, and sending can disturb simultaneous acquisition. |
| CAN during probing | The driver is reconfigured for about 1.2 s per candidate rate (500/250/125/1000 kbit/s); frames can be lost and counters restart. Without results the initial speed is restored. |
| CAN FD | Not supported; classic CAN only. |
| A sent CAN frame is not confirmed | `twai_transmit()` success means the frame was queued (single-shot), not that a recipient acknowledged it. In normal mode the node still sends ACKs for other frames. |
| Dashboard unreachable | Confirm you are on the `TraceLink-ESP32` network and kept the connection despite the no-Internet warning; the page is only served at `http://192.168.4.1`. |
| Control request rejected | 403 means the page's boot token is stale (reload after an ESP32 reboot); 409 means TX is armed/disarmed incorrectly for the action or another operation is running; 400 means invalid payload, rate, or CAN fields. |
| Serial payload limits | HEX up to 64 bytes per send (8 for CAN); text 1–64 UTF-8 bytes; firmware enforces these limits too. |
| Not a security boundary | Local HTTP plus the Wi-Fi password protects against accidents, not attackers; there is no TLS and no per-user authentication. Do not use it on sensitive or production equipment. |

Hardware verification status, known issues, and resume points are maintained in [PROJECT_STATUS.md](PROJECT_STATUS.md) and [TODO.md](TODO.md).

## Development

Toolchain: PlatformIO Core with platform `espressif32@6.10.0`, board `esp32dev`, Arduino framework 2.0.17. Build flags: `-Wall -Wextra`. There is no CI workflow in the repository.

```sh
pio run                     # firmware build (also regenerates web_page.h)
pio run --target upload     # flash, after identifying the port
pio device monitor --baud 115200
```

Host-side tests (no board needed), run from the repository root:

```sh
g++ -std=c++11 -Wall -Wextra -Werror -Iinclude test/native/history_test.cpp -o tracelink-history-test
./tracelink-history-test    # Windows: tracelink-history-test.exe
```

These cover the ring history, HEX formatting, parsers, rate tables, and baud estimation. They are unit tests of hardware-independent logic; they say nothing about physical bus behavior. For web changes, the inline script is also checked with `node --check`, and dashboard behavior has been exercised in a browser against mocked API responses.

Conventions: tabs for indentation, Doxygen `@file`/`@brief` headers, JSDoc on dashboard functions, firmware files under 400 lines and functions under 50 executable lines, Conventional Commits in English, no committed secrets, no push without an explicit request. Agent-facing rules live in [agent.md](agent.md).

| Guide | Contents |
| --- | --- |
| [docs/README.md](docs/README.md) | Documentation index and reading paths |
| [docs/usage.md](docs/usage.md) | Dashboard workflow and HTTP API usage examples |
| [docs/architecture.md](docs/architecture.md) | Tasks, state ownership, buffers, scan and TX state machines |
| [docs/api.md](docs/api.md) | `/api/events`, `/api/systems`, `/api/control` reference |
| [docs/hardware.md](docs/hardware.md) | Boards, modules, wiring, electrical boundaries, validation status |
| [docs/testing.md](docs/testing.md) | Test commands, coverage, and what remains unverified |
| [docs/troubleshooting.md](docs/troubleshooting.md) | Observed issues and diagnostics |
| [PROJECT_STATUS.md](PROJECT_STATUS.md) | Verification status, limits, and resume point for maintainers |
| [TODO.md](TODO.md) | Done/pending work and milestones |
| [CHANGELOG.md](CHANGELOG.md) | Dated history of notable changes |

## Credits and license

Maintained by **Camiu** ([@camiu01](https://github.com/camiu01)). Built on the Arduino ESP32 core and the ESP-IDF TWAI driver, via PlatformIO.

TraceLink is released under the GNU General Public License v3.0; see [LICENSE](LICENSE).
