# Testing and validation

[← README](../README.md) · [Docs index](README.md)

Paths are relative to the repository root. This document distinguishes checks that have been executed from procedures that still need to be performed.

## Host-side checks (no board required)

```sh
# Firmware build (also regenerates web_page.h from web/)
pio run

# Native unit tests
g++ -std=c++11 -Wall -Wextra -Werror -Iinclude test/native/history_test.cpp -o tracelink-history-test
./tracelink-history-test
g++ -std=c++11 -Wall -Wextra -Werror -Iinclude test/native/control_usb_test.cpp -o tracelink-control-usb-test
./tracelink-control-usb-test

# Dashboard inline-script syntax and script-level regressions
node --test test/web/dashboard.test.mjs
```

`test/native/history_test.cpp` covers the hardware-independent logic: ring history (ordering, overwrite accounting, cursor reads), HEX formatting, the strict HEX parser, the overflow-checked number parser, the supported-rate tables, and baud estimation including glitch rejection. These are unit tests of data structures and parsers; they do not exercise drivers, Wi-Fi, HTTP, or any physical bus.

`.github/workflows/ci.yml` runs these checks plus `npm ci` and `npm test` in
`mcp/`, on Ubuntu 24.04 with Python 3.11, Node.js 24.13.1 and PlatformIO Core
6.1.19. Dependencies come from `mcp/package-lock.json`; no hardware, real ports,
credentials or paid services are required. Installation needs public package
registries. The workflow was prepared but not executed remotely during the audit.

The dashboard test parses and executes the shipped inline script using Node's
`vm.Script` and a minimal DOM, with simulated fetch responses and no network.
It covers language fallback, persistent control results, separate device status,
paused safety refresh, reboot reset, safe text rendering, autoscroll and CSV
formula/quote protection. It does not establish real browser layout or HTTP behavior.

The USB/control test covers the shared HTTP/USB command parser, percent decoding,
payload/ID/DLC limits, all busy/scanning/armed admission combinations, cursor
limits, independent CRC vectors and buffer overflow/timeout resynchronization.
It does not exercise UART hardware or FreeRTOS scheduling.

For the MCP bridge run `npm test` from `mcp/`. The suite includes actual stdio/HTTP
MCP clients, local HTTP mocks and a simulated OS serial binding. No real COM port
is opened. See [mcp.md](mcp.md) for setup and Postman simulation commands.

## Checks executed so far

| Check | Result | Date |
| --- | --- | --- |
| Audit baseline firmware, both native suites, MCP suite | Build and native tests passed; MCP 38/38 | 2026-11-17 |
| Dashboard regression suite | 3 assertions failed before fixes; final 6/6 passed | 2026-11-17 |
| Isolated Chromium, simulated fetch | Startup, text rendering, 403 persistence, CSV, paused safety, reboot, offline controls and single POST passed | 2026-11-17 |
| Clean isolated MCP install | `npm ci --no-audit --no-fund` then `npm test`: 38/38; temporary copy removed | 2026-11-17 |
| Final firmware with dashboard fixes | Build passed; RAM 66,704 B, flash 830,413 B; no upload | 2026-11-17 |
| MCP HTTP/stdio and mocked COM suite (`npm test`, Node.js 24.13.1) | 38 passed; no physical serial port opened | 2026-11-10 |
| Native history and shared control/USB protocol tests (`-std=c++11 -Wall -Wextra -Werror`) | Both passed | 2026-11-10 |
| USB-enabled firmware `pio run` | Success; RAM 66,704 bytes, flash 830,205 bytes; not uploaded | 2026-11-10 |
| `pio run` (PlatformIO Core 6.1.19, `espressif32@6.10.0`, Arduino 2.0.17) | Success; RAM 20.0%, flash 62.5% | 2026-10-22 |
| Native tests with `g++ -std=c++11 -Wall -Wextra -Werror` | Passed | 2026-10-22 |
| Dashboard inline script `node --check` | Passed | 2026-10-22 |
| Dashboard in a browser against mocked `/api/events` responses (autoscroll, suspension on scroll-up, resume, pause/clear) | Passed | 2026-10-22 |
| Upload to ESP32-D0WD-V3 over USB (COM7), hash-verified | Success | 2026-10-22 |
| Serial boot log: receive-only banner and AP announcement | Observed | 2026-10-22 |

## Not yet verified

- USB/COM MCP on the board, including driver-related reset behavior and actual firmware queue timing.
- Wi-Fi authentication against the AP and full dashboard behavior served by the device.
- CAN on a physical bus. The user reported RS232 and RS485 working on 17 November
  2026; actual test date, firmware and RX/TX coverage were not recorded, and were
  not independently re-verified.
- TX arm/disarm behavior, 60 s auto-disarm, fail-closed partial enable, and CAN return to listen-only on real hardware.
- Baud estimation against a known source at several speeds.
- CAN bus-off/error recovery and driver restart behavior.
- Behavior under load and real loss rates.

## Manual hardware procedures (not performed in this audit)

Use the single [hardware checklist](hardware.md#before-any-future-hardware-trial-user-executed):
identify hardware, check it unpowered, verify supplies before signals, boot USB-only,
then acquire one known source/interface at a time. It specifies evidence and pass
criteria; TX and electrical measurements require separate authorization/procedures.

Hardware trials require the safety preconditions in [hardware.md](hardware.md) and [agent.md](../agent.md): identified board and port, confirmed wiring and voltages, and explicit authorization before flashing or transmitting.
