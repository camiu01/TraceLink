# Firmware architecture

Paths are relative to the repository root. This document describes the architecture as implemented, not an intended redesign.

## Overview

TraceLink is a single-partition Arduino/ESP32 firmware. `setup()` initializes the buses, Wi-Fi access point, and HTTP server, then starts two FreeRTOS tasks. There is no persistent storage: all state lives in RAM and is lost on reboot.

```text
RS232 module --UART2--> UartBus --+
RS485 module --UART1--> UartBus --+--> acquire task --> CaptureHistory (128 records, RAM)
CAN module   --TWAI---> CanBus  --+        |                |
                                           | commands       +--> usbOutput task --> USB serial log
HTTP clients <--> loop()/WebServer <-------+                |
                     (validate + enqueue)                   +--> /api/events polling --> dashboard
```

## Tasks and boundaries

| Execution context | Role | Stack / priority |
| --- | --- | --- |
| `acquire` task | Owns all bus adapters. Polls each bus, pushes records into the history, advances the speed scan, executes queued commands (rate change, arm/disarm, send). | 6144 bytes, priority 2 |
| `usbOutput` task | Drains the history to the USB serial log in batches of 4 every 5 ms. | 4096 bytes, priority 1 |
| Arduino `loop()` | Runs `server.handleClient()` with a 2 ms delay; HTTP handlers only validate and enqueue work. | Arduino default |
| GPIO interrupt (`sampleEdge`) | During serial baud sampling only, fills pulse-interval histogram bins. ISR work is limited to a critical-section histogram update. | ISR |

One mutex (`xSemaphoreCreateMutex`) protects the history, the status block, and the pending command. Readers (`usbOutput`, HTTP handlers) take the same mutex, so a consistent snapshot is always served.

## Module responsibilities

| Module | Responsibility |
| --- | --- |
| `include/config.h` | Compile-time constants: GPIOs, initial speeds, buffer sizes, AP credentials, TX window. `include/local_config.h` (git-ignored) overrides the password. |
| `include/capture.h` | `Capture` record (sequence, timestamp, source, CAN id/flags/DLC, up to 64 data bytes), `CaptureHistory<128>` ring with per-source counters and overwrite accounting, `formatHex`. |
| `include/commands.h` | Hardware-independent logic: strict HEX parser, overflow-checked integer parser, supported rate tables (serial 1200–115200, CAN 125k–1M), `estimateBaud` over a `PulseHistogram`. Covered by host tests. |
| `include/bus_interface.h` | `BusInterface` contract (`begin`, `receive`, `send`, `setTransmit`, `health`, `rate`, `protocol`) plus `UartBus` and `CanBus`. |
| `src/bus_interface.cpp` | UART/RS485 driver with optional DE/RE toggling around sends; TWAI driver with listen-only/normal mode switching, single-shot transmission, and health counters from `twai_get_status_info()`. |
| `src/TraceLink.cpp` | Adapter registry, acquisition and USB tasks, speed-scan state machine, TX arm/timeout logic, HTTP API, JSON rendering. |
| `web/index.html` | Dashboard: 250 ms polling of `/api/events`, interface cards built from API descriptors, filters, pause/autoscroll, CSV export, send form. All bus bytes are rendered as text, never as HTML. |
| `web/systems.json` | Device profiles; schema validated at build time by the embed script. |
| `scripts/embed_web.py` | PlatformIO `pre:` script that packs `web/` into a generated `web_page.h` (raw string + JSON), so no filesystem image is needed. |

## Buffers and ownership

- `CaptureHistory` is a fixed 128-slot ring (`include/config.h`), guarded by the mutex. Producers: the acquisition task (and, for sends, the queued command path). Consumers: the USB task and the `/api/events` handler. Overwritten records are counted, and readers learn about gaps through the `lost` counter.
- Each UART has a 2048-byte RX ring buffer from the Arduino core. UART overflow is not counted.
- CAN uses the TWAI driver queues: RX queue length 64, TX queue length 4 when armed (0 in listen-only).
- `/api/events` responses are built in a heap `String` reserved to 14000 bytes, with a fixed batch of 32 events.

## Speed-scan state machine

`scan` (from the dashboard) is refused while TX is armed. The scan has two independent parts:

1. **Serial:** GPIO interrupts on both RX lines collect edge-interval histograms for 3 s. `estimateBaud` picks the highest supported rate with at least six bins near its bit time, requiring at least 20 edges. Results are reported as estimates and never applied automatically.
2. **CAN:** the TWAI driver is reinstalled at 500, 250, 125, and 1000 kbit/s, spending about 1.2 s per rate in listen-only mode. Three received frames lock in the current rate; otherwise the initial rate is restored.

Both parts run inside the acquisition task, so acquisition continues (with possible loss during CAN reconfiguration, which also resets the driver's counters).

## TX state machine

TX is disabled at boot. `arm` calls `setTransmit(true)` on every adapter; if any adapter fails, all are forced back to disabled (fail-closed). CAN arming reinstalls the driver in normal mode, which means the node ACKs bus traffic even without user commands. Each accepted send refreshes a 60 s idle timer; on expiry the acquisition task disarms TX automatically. Sends are executed once; HTTP errors never trigger retries, and CAN frames are queued single-shot (`ss = 1`), so a missing ACK does not cause retransmission.

## Initialization and failure handling

Setup order: USB serial, boot token (`esp_random`), mutex, bus adapters, Wi-Fi AP, HTTP routes, tasks. Failure to create the mutex or the acquisition task halts the board with a message on the serial log. A failed Wi-Fi AP start is logged and the board continues with USB-only output. CAN driver install/start failures are logged with the ESP-IDF error name and reported as `non disponibile` in the API.

## HTTP layer

Handlers live in `src/TraceLink.cpp`. `POST /api/control` validates the `X-TraceLink` boot token header (no CORS is enabled), parses and bounds-checks every field, rejects conflicting states with 409, and enqueues the command for the acquisition task, answering 202. The actual outcome appears in the next `/api/events` poll as the `message` field. See [api.md](api.md) for the full reference.
