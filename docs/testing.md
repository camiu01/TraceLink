# Testing and validation

Paths are relative to the repository root. This document distinguishes checks that have been executed from procedures that still need to be performed.

## Host-side checks (no board required)

```sh
# Firmware build (also regenerates web_page.h from web/)
pio run

# Native unit tests
g++ -std=c++11 -Wall -Wextra -Werror -Iinclude test/native/history_test.cpp -o tracelink-history-test
./tracelink-history-test

# Dashboard JavaScript syntax
node --check <extracted inline script>
```

`test/native/history_test.cpp` covers the hardware-independent logic: ring history (ordering, overwrite accounting, cursor reads), HEX formatting, the strict HEX parser, the overflow-checked number parser, the supported-rate tables, and baud estimation including glitch rejection. These are unit tests of data structures and parsers; they do not exercise drivers, Wi-Fi, HTTP, or any physical bus.

There is no CI workflow in the repository; each run above is a local check.

## Checks executed so far

| Check | Result | Date |
| --- | --- | --- |
| `pio run` (PlatformIO Core 6.1.19, `espressif32@6.10.0`, Arduino 2.0.17) | Success; RAM 20.0%, flash 62.5% | 2026-10-22 |
| Native tests with `g++ -std=c++11 -Wall -Wextra -Werror` | Passed | 2026-10-22 |
| Dashboard inline script `node --check` | Passed | 2026-10-22 |
| Dashboard in a browser against mocked `/api/events` responses (autoscroll, suspension on scroll-up, resume, pause/clear) | Passed | 2026-10-22 |
| Upload to ESP32-D0WD-V3 over USB (COM7), hash-verified | Success | 2026-10-22 |
| Serial boot log: receive-only banner and AP announcement | Observed | 2026-10-22 |

## Not yet verified

- Wi-Fi authentication against the AP and full dashboard behavior served by the device.
- RS485 and CAN on physical buses; RS232 capture was reported working by the user but not independently re-verified.
- TX arm/disarm behavior, 60 s auto-disarm, fail-closed partial enable, and CAN return to listen-only on real hardware.
- Baud estimation against a known source at several speeds.
- CAN bus-off/error recovery and driver restart behavior.
- Behavior under load and real loss rates.

## Manual hardware procedures (to be performed, in order)

1. Boot with USB only, transceivers detached, TX never armed; check the boot log and page load.
2. RS232 against a known source at a known speed; compare HEX bytes.
3. RS485 on a live line, RX first; TX only after confirming device, wiring, speed, and the exact command.
4. CAN on a terminated two-node bench; check identifiers, DLC, data, and loss counters under load.
5. Speed-scan trials with known baud rates, recording wrong estimates.

Hardware trials require the safety preconditions in [hardware.md](hardware.md) and [agent.md](../agent.md): identified board and port, confirmed wiring and voltages, and explicit authorization before flashing or transmitting.
