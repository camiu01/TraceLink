# TRACELINK

**Multi-bus passive monitor for ESP32: RS232, RS485 and classic CAN capture with an offline web dashboard and explicitly-armed transmission.**

TraceLink receives RS232 on UART2, RS485 on UART1 and classic CAN on the built-in TWAI controller at the same time. It shows traffic and loss counters on the USB monitor and on a page served by the ESP32 access point, with no Internet and no external web dependencies. Transmission stays disabled at boot and requires explicit arming with automatic disarm 60 seconds after the last send.

## What's new on 2026-10-22

Build is green after the user lifted the pause and authorized firmware work:

- **Firmware compiles:** latest PlatformIO build passed (RAM 20.0%, Flash 62.5%); `web_page.h` regenerates from `web/` on every build.
- **Dashboard autoscroll:** follows new traffic, stops when scrolling up, and resumes with the checkbox. Browser checks with simulated APIs, native C++ tests, and JavaScript syntax checks passed. This dashboard update has not yet been uploaded.
- **Arduino C++11 fixes:** `BusHealth` constructor replaces brace init rejected under `gnu++11`; explicit `twai_general_config_t` / `twai_timing_config_t` / `twai_filter_config_t` replace `auto` on TWAI macros.
- **Validated wiring map:** isolated auto-direction RS485 module (`TX -> GPIO26`, `RX <- GPIO27`, GPIO25 unconnected) and SP3232 RS232 module (crossed `TXD -> GPIO16`, `RXD <- GPIO17`) at 3.3 V; UART0 stays free for USB flash and monitor.
- **English documentation:** README, project status and agent instructions translated; Doxygen file/function headers plus JSDoc on the dashboard; tabs for indentation.
- **Hardware bring-up:** previous firmware uploaded successfully on COM7; serial boot reported RX-only and the access point. The user reports working capture. Wi-Fi authentication and real bus behavior have not been independently verified.

These are bench-tool readings, not certified measurements. See [PROJECT_STATUS.md](PROJECT_STATUS.md) for limits and the safety rules.

## Previously: TX-capable monitor

The first sketch was RX-only. The current firmware keeps RX-only at boot but adds web-driven transmission behind explicit arming: `POST /api/control` with HEX/text/CAN-ID validation, per-bus adapters owned by the acquisition task, single-shot CAN queueing with no delivery claim, and listen-only CAN whenever TX is off.

The full checklist lives in [TODO.md](TODO.md).

## Features

- **Three buses at once** — UART2 RS232, UART1 RS485 and TWAI CAN sampled by one acquisition task into a 128-record ring history.
- **Offline dashboard** — dynamic interface cards, RX/TX view, filters, view pause/clear and CSV export with spreadsheet-formula neutralization; works on the AP without Internet. Autoscroll follows new records, stops when you scroll up, and resumes using the checkbox.
- **Speed probing** — serial pulse-timing estimate (confirmation required, never auto-applied) plus listen-only CAN search over 500/250/125/1000 kbit/s.
- **Armed sending only** — TX disabled at boot, explicit arm with confirm dialog, 60 s idle auto-disarm, fail-closed on partial enable, no automatic resends.
- **Device profiles** — `web/systems.json` presets fill send fields and speeds; loading a profile never arms TX and never sends.
- **Loss accounting** — history evictions, CAN missed/overrun/error counters and browser-side gap counts surfaced in the UI and API.
- **USB mirror** — `#seq ms SOURCE RX/TX` log with HEX plus CAN id/flags/DLC at 115200 baud.
- **Embedded assets** — `scripts/embed_web.py` packs HTML and profiles into flash; no SPIFFS/LittleFS upload step.

## Bus engine

TraceLink keeps hardware-dependent code behind one adapter contract:

| Module | Key role |
|---|---|
| `bus_interface.h`, `bus_interface.cpp` | `BusInterface` contract; `UartBus` (UART + optional DE/RE) and `CanBus` (TWAI listen-only/normal) |
| `capture.h` | `Capture` record, `Source` names, `CaptureHistory` ring, `formatHex` |
| `commands.h` | Strict HEX parser, overflow-checked numbers, rate tables, `estimateBaud` pulse histogram |
| `TraceLink.cpp` | Task registry, speed scan, TX arm/timeout state machine, `/api/events` + `/api/control` |
| `web/index.html` | Polling, dynamic cards, send form, CSV export (text-only rendering of bus bytes) |
| `web/systems.json` | Example device profile (adapt to a real documented protocol) |
| `scripts/embed_web.py` | Build-time `web_page.h` generation with profile validation |

## Wiring

UART0 (GPIO1/GPIO3) must stay free for USB flashing and the monitor. Power both TTL sides at 3.3 V, never 5 V: ESP32 GPIOs are not 5 V tolerant.

| ESP32 | RS232 SP3232 module (TTL side) | RS485 isolated module (TTL side) |
|---|---|---|
| 3V3 | VCC | VIN |
| GND | GND | GND (TTL side only) |
| GPIO16 / RX2 | Module TXD | — |
| GPIO17 / TX2 | Module RXD (optional if RX-only) | — |
| GPIO26 / UART1 RX | — | Module TX |
| GPIO27 / UART1 TX | — | Module RX |
| GPIO25 | — | Unconnected (auto-direction module) |

Bus sides (leave disconnected until the USB-only first boot passes): RS232 device TX to module RXD plus signal ground; RS485 A/B twisted pair with the earth terminal floating and 120 Ω only at the line ends; CANH/CANL to an SN65HVD230-style 3.3 V module (GPIO32 TX, GPIO33 RX) on a bus with two active nodes. The B0505XT-1WR3 DC/DC is not needed: the RS485 module already integrates isolation.

Conventions: tabs, single quotes, semicolons, Doxygen `@file`+`@brief` headers and JSDoc on functions, firmware files <400 lines, functions <50 lines (`src/TraceLink.cpp` split still pending, see TODO).

## Run it

Prerequisites: PlatformIO (`.venv` in the project), a data USB cable, and `include/local_config.h` with your own AP password (8-63 chars, see README history note in TODO):

```bash
.venv/bin/pio run
.venv/bin/pio run --target upload   # only on the identified board port, wiring confirmed
.venv/bin/pio device monitor --baud 115200
```

Checks:

```bash
g++ -std=c++11 -Wall -Wextra -Werror -Iinclude test/native/history_test.cpp -o /tmp/tracelink-history-test
/tmp/tracelink-history-test
```

Connect to `TraceLink-ESP32`, open `http://192.168.4.1`, and keep the connection when the OS warns there is no Internet. First boot is USB-only with transceivers detached; enable TX only after confirming wiring, speed and the exact command with the device owner.

## Structure

```text
include/
  config.h                       # build settings, GPIO map, TX window
  capture.h                      # records, sources, ring history, HEX
  commands.h                     # parsers, rate tables, baud estimate
  bus_interface.h                # BusInterface contract, UartBus, CanBus
src/
  TraceLink.cpp                  # tasks, scan, HTTP API (split pending)
  bus_interface.cpp              # UART/RS485 + TWAI drivers
web/
  index.html                     # offline dashboard (no external deps)
  systems.json                   # example device profile
scripts/
  embed_web.py                   # packs web/ into web_page.h
test/
  native/history_test.cpp        # host-side unit checks, no board needed
```

## Safety

Do not infer wiring from the USB port. Identify board and port, ask what hangs on every GPIO, and get explicit upload authorization before flashing: no full erase, no eFuse, no overclock. Automated tests stay in receive mode; CAN normal mode ACKs even without user commands. Local HTTP plus the Wi-Fi password is not industrial protection.

## License

Maintained by **Camiu** ([@camiu01](https://github.com/camiu01)). No license file yet — add one before any public release.
