# Shared instructions for TraceLink

These instructions apply to all agents working in the project. `AGENTS.md` refers to this file.

## Before starting

1. Read `PROJECT_STATUS.md` to learn what was written, what was verified, and what is missing.
2. Check the real files: the document does not replace verifying the current state.
3. Respect the pause request. Resume implementation, build, or hardware tests only when the user asks for it.
4. Communicate in Italian, briefly and precisely.

The project is called **TraceLink**. Current target: classic ESP32 / ESP32-WROOM-32, PlatformIO and Arduino framework.

## Hardware safety

The user has explicitly asked not to damage the ESP32.

- Do not infer wiring or voltages from the presence of a USB port.
- Before flashing firmware or changing GPIOs, identify the model and port and ask what is connected to the pins.
- Do not flash on an automatically selected port without identifying the board.
- Do not run full erase, eFuse changes, overclocking, or supply variations without specific authorization.
- Do not connect RS232, RS485, or CAN signals directly to the GPIOs.
- GPIOs are 3.3 V and are not 5 V tolerant. Verify logic levels and module supply.
- The B0505XT-1WR3 is a DC/DC converter, not a transceiver and not a signal isolator.
- GPIO16/17 wiring applies to WROOM-32, not automatically to WROVER or other variants.
- During automated tests stay in receive mode: no TX on real hardware without authorization on the exact command and destination device.
- CAN in normal mode sends ACKs even without user commands: explain this effect before enabling it.
- If the board, cable, permissions, transceiver, or verified wiring are missing, declare the blocker. Do not probe connections blindly.

## Architecture

| File | Responsibility |
| --- | --- |
| `include/config.h` | Build settings and GPIOs. |
| `include/capture.h` | Records, source names, and history. |
| `include/commands.h` | Parser and baud estimation, testable without ESP32. |
| `include/bus_interface.h` | Common adapter contract. |
| `src/bus_interface.cpp` | UART/RS485/CAN drivers. |
| `src/TraceLink.cpp` | Acquisition, interface registry, scanning, TX lifecycle and setup. |
| `src/api.cpp` | Shared snapshots/control queue and HTTP handlers. |
| `src/usb_console.cpp` | Framed USB API and serialized USB capture log. |
| `include/control_request.h` / `include/usb_protocol.h` | Hardware-independent control validation and USB framing/parsing. |
| `web/index.html` | Local dashboard, no external dependencies. |
| `web/systems.json` | Reusable device and command profiles. |
| `scripts/embed_web.py` | Asset embedding into the firmware. |
| `test/native/history_test.cpp` | Local C++ tests without a board. |

Keep parsing and hardware-independent logic testable on the PC. Adapters belong to the acquisition task: HTTP handlers must validate and forward requests, not drive the buses directly.

Do not introduce frontend frameworks, libraries, or cloud services unless the request needs them. The page must work on the access point without Internet.

### Adding a system

For a device using the existing ports, prefer a profile in `web/systems.json` with name, bus, speed, and documented commands. Do not present invented commands as the real commands of a device.

Loading a profile must only prepare the fields; it must not enable TX, apply dangerous changes, or send automatically.

For a new physical interface, implement a `BusInterface` adapter, add the source, and register it. Also check HTTP validation, allowed speeds, UI descriptors, hardware limits, and tests. Do not promise extensions without code for new protocols.

## Transmission and speed control

- TX must be disabled at boot and require explicit enabling.
- Preserve manual disable, automatic timeout, and fail-safe behavior on partial errors.
- Never resend a command automatically after an HTTP timeout or error.
- Validate payloads, lengths, IDs, and DLC in the firmware too, not only in the browser.
- Do not present a queued CAN frame as delivered or acknowledged by the recipient.
- A silent line does not allow determining the speed.
- Present the serial baud as an estimate and require confirmation; do not assume parity or protocol.
- Do not scan CAN in normal mode: use listen-only.
- Document possible losses during scanning, reconfiguration, and sending.

## Data and access

- Do not add personal passwords to code or documents. Use `include/local_config.h`, excluded from Git.
- Change the public example password before real use.
- Bus data is untrusted input: render it as text, not HTML.
- Keep formula protection in CSV exports.
- Do not enable indiscriminate CORS. Keep the token/header check for mutating requests.
- Local HTTP and the Wi-Fi password are not sufficient protection for critical devices. Report the limits, do not promise industrial security.

## Validation and delivery

To validate code changes, run at least:

```sh
.venv/bin/pio run
g++ -std=c++11 -Wall -Wextra -Werror -Iinclude test/native/history_test.cpp -o /tmp/tracelink-history-test
/tmp/tracelink-history-test
```

If `.venv` is unavailable, use the PlatformIO installed in the environment, without globally modifying the system.

For web changes also verify JavaScript syntax and behavior in a browser with simulated APIs. For a documentation-only change, content verification and path consistency are enough, without starting builds or hardware.

Always distinguish:

- written code;
- passed local tests;
- successful firmware build;
- successful upload;
- hardware-verified behavior.

Never claim as verified what is only implemented. Report errors and skipped verifications. Update `PROJECT_STATUS.md` and the README when needed. Do not delete existing work and do not publish or push without a request.

When suspending work, do not leave builds, servers, or agent-started tests running in the background. Record the resume point and stop.
