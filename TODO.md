# TODO

> Last reviewed: 2026-11-23

Implementation is not verification. See [PROJECT_STATUS.md](PROJECT_STATUS.md)
for the matrix and historical evidence, and [audit findings](docs/audit-2026-11-17.md)
for source-level risks. Hardware trials are not authorized by this checklist.

## Hardware validation

Use the [hardware checklist](docs/hardware.md#before-any-future-hardware-trial-user-executed)
rather than repeating wiring instructions here.

- [ ] **Record board and module identity** — exact DevKit/PCB revisions, USB connector/cable, converter IC markings and schematics.
- [ ] **Confirm electrical characteristics** — module supply/current, receiver logic levels, signal direction, DE//RE,
  RS485 polarity, termination/bias, signal isolation and power isolation separately.
- [ ] **Identify reference connections** — reference/ground/shield/earth connections; no universal bonds or floating terminals.
- [ ] **Document current wiring** — photos and measurements; historic user confirmation
  does not establish voltage, continuity or isolation.
- [ ] **Board, port and upload authorization** — Needs hardware: identify current board/port and obtain separate upload authorization, if needed.
- [ ] **USB-only boot check** — Needs hardware: selected build, TX disarmed; verify AP authentication/page.
- [ ] **RX-only RS232** — Needs hardware: known source, exact bytes and several known baud patterns.
- [ ] **RX-only RS485** — Needs hardware: isolated bench, known source, exact bytes, documented driver-disabled state.
- [ ] **CAN bench** — Needs hardware: terminated bench with two active peers; ID/flags/DLC/data and reference peer logs.
- [ ] **Bus faults on real hardware** — Needs hardware: cover bus-off, driver restarts, UART overflow and history losses.
- [ ] **TX trials** — Blocked: separately authorize exact TX trials, timeout/partial-error behavior and CAN
  normal-mode ACK, single-shot and return-to-listen-only checks.
- [ ] **Evidence format** — record date, firmware version/commit, configuration, topology, logs, expected
  outcomes and pass/fail; use **not recorded** where evidence is missing.
- [x] **Historical first upload** — RX-only boot documented on 22 October 2026.
  This is not an upload of the current USB-enabled source.
- [x] **User-reported RS232/RS485 operation** — commercial module descriptions
  recorded on 17 November 2026; detailed controlled test evidence remains pending.

## Firmware

- [ ] **TX idle expiry** — Blocked: pending approval; decide and test expiry-before-send behavior at the 60 s TX idle boundary.
- [ ] **Fail-closed handling** — Blocked: pending approval; define it for send/rate failures and failed CAN stop/uninstall.
- [ ] **Driver/task test harness** — cover approved safety changes with a harness, not just parser/mocked bridge tests.
- [ ] **CAN error log over TL1** — serialize the CAN installation-error log with TL1 output without blocking
  acquisition on slow USB output; test concurrent error/reply output.
- [ ] **Bounded waits under load** — define bounded driver/output waits and measure acquisition/HTTP delays under load.
- [x] **PlatformIO project** — classic ESP32, pinned `espressif32@6.10.0`.
- [x] **Bus adapters** — UART/RS485 and TWAI adapters; acquisition-task ownership and shared control queue.
- [x] **History** — 128-record RAM history, sequences, cursors and overwrite accounting.
- [x] **Validation and rates** — HEX/number validation, supported-rate tables, serial pulse-based baud estimate.
- [x] **TX safety defaults** — software TX boot default, arm/disarm, idle timer and single-shot CAN queueing.
  This does not establish physical fail-closed behavior or timing under all failures.

## APIs and MCP bridge

- [ ] **Malformed CAN flags** — Blocked: pending approval; decide whether malformed CAN `extended`/`rtr` values must return 400 rather than
  silently becoming false; preserve omitted-field compatibility deliberately.
- [ ] **Real MCP transport checks** — Needs hardware: real MCP HTTP/USB reads, USB reset/port contention and disconnect behavior.
- [x] **HTTP and TL1 USB APIs** — boot token, bounded USB requests/CRC and strict USB forms.
- [x] **MCP bridge tests** — MCP HTTP/stdio and mock-serial tests.

## Web dashboard

- [ ] **Browser coverage** — test storage-disabled browser behavior, mobile layout, long-session pause,
  filtered export and profile flow. Script-level tests are not layout tests.
- [ ] **Real profile commands** — replace illustrative profile commands only when actual device documentation exists.
- [x] **Offline bilingual dashboard** — filters, pause/autoscroll, CSV formula protection.
- [x] **Profiles** — fill fields only; bundled commands are examples, not a real-device protocol.
- [x] **Startup fixes** — dashboard invalid-language startup and polled-away request results fixed with regressions.

## Repository, CI and docs

- [ ] **Keep local passwords out of commits** — a pre-existing change is in tracked
  `include/config.h`; do not stage it accidentally. Prefer the ignored local override.
- [ ] **Untracked local files** — do not stage pre-existing `.factory/` or `security-findings.json`.
- [ ] **Commit review** — review diff and validation before any separately requested Conventional Commit.
- [ ] **Conventions** — review function/documentation conventions when touching code, not by broad reformatting.
- [x] **Build and tests** — embedded assets, native parser/history tests; local build and baseline tests passed on 17 November 2026.
- [x] **CI workflow** — minimal hardware-free workflow prepared.
- [x] **Status and hardware docs** — status reorganized; canonical conditional hardware guide and preflight checklist.
- [x] **License and language** — GPLv3 license and English documentation.
