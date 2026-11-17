# TODO

Implementation is not verification. See [PROJECT_STATUS.md](PROJECT_STATUS.md)
for the matrix and historical evidence, and [audit findings](docs/audit-2026-11-17.md)
for source-level risks. Hardware trials are not authorized by this checklist.

## Implemented / checked locally

- [x] Classic ESP32 PlatformIO project, pinned `espressif32@6.10.0`.
- [x] UART/RS485 and TWAI adapters; acquisition-task ownership and shared control queue.
- [x] 128-record RAM history, sequences, cursors and overwrite accounting.
- [x] HEX/number validation, supported-rate tables, serial pulse-based baud estimate.
- [x] Software TX boot default, arm/disarm, idle timer and single-shot CAN queueing.
  This does not establish physical fail-closed behavior or timing under all failures.
- [x] HTTP and TL1 USB APIs, boot token, bounded USB requests/CRC and strict USB forms.
- [x] Offline bilingual dashboard, filters, pause/autoscroll, CSV formula protection.
- [x] Profiles fill fields only; bundled commands are examples, not a real-device protocol.
- [x] Embedded assets, native parser/history tests, MCP HTTP/stdio and mock-serial tests.
- [x] Local build and baseline tests passed on 17 November 2026.
- [x] Dashboard invalid-language startup and polled-away request results fixed with regressions.
- [x] Minimal CI workflow prepared; remote execution not performed.
- [x] Status reorganized; canonical conditional hardware guide and preflight checklist.
- [x] GPLv3 license and English documentation.
- [x] Historical first upload and RX-only boot documented on 22 October 2026.
  This is not an upload of the current USB-enabled source.
- [x] User-reported RS232/RS485 operation and commercial module descriptions
  recorded on 17 November 2026; detailed controlled test evidence remains pending.

## Pending approval: safety / compatibility changes

- [ ] Decide and test expiry-before-send behavior at the 60 s TX idle boundary.
- [ ] Define fail-closed handling for send/rate failures and failed CAN stop/uninstall.
- [ ] Decide whether malformed CAN `extended`/`rtr` values must return 400 rather than
  silently becoming false; preserve omitted-field compatibility deliberately.
- [ ] Cover approved changes with a driver/task harness, not just parser/mocked bridge tests.

## Reliability and UI follow-up

- [ ] Serialize the CAN installation-error log with TL1 output without blocking
  acquisition on slow USB output; test concurrent error/reply output.
- [ ] Define bounded driver/output waits and measure acquisition/HTTP delays under load.
- [ ] Cover bus-off, driver restarts, UART overflow and history losses on real hardware.
- [ ] Test storage-disabled browser behavior, mobile layout, long-session pause,
  filtered export and profile flow. Script-level tests are not layout tests.
- [ ] Replace illustrative profile commands only when actual device documentation exists.
- [ ] Review function/documentation conventions when touching code, not by broad reformatting.

## Hardware identification and validation

Use the [hardware checklist](docs/hardware.md#before-any-future-hardware-trial-user-executed)
rather than repeating wiring instructions here.

- [ ] Record exact DevKit/PCB revisions, USB connector/cable, converter IC markings and schematics.
- [ ] Confirm module supply/current, receiver logic levels, signal direction, DE//RE,
  RS485 polarity, termination/bias, signal isolation and power isolation separately.
- [ ] Identify reference/ground/shield/earth connections; no universal bonds or floating terminals.
- [ ] Record current wiring with photos and measurements; historic user confirmation
  does not establish voltage, continuity or isolation.
- [ ] Identify current board/port and obtain separate upload authorization, if needed.
- [ ] USB-only boot of the selected build, TX disarmed; verify AP authentication/page.
- [ ] RX-only RS232 known source: exact bytes and several known baud patterns.
- [ ] RX-only RS485 isolated bench: known source, exact bytes, documented driver-disabled state.
- [ ] CAN terminated bench with two active peers: ID/flags/DLC/data and reference peer logs.
- [ ] Real MCP HTTP/USB reads, USB reset/port contention and disconnect behavior.
- [ ] Separately authorize exact TX trials, timeout/partial-error behavior and CAN
  normal-mode ACK, single-shot and return-to-listen-only checks.
- [ ] Record date, firmware version/commit, configuration, topology, logs, expected
  outcomes and pass/fail; use **not recorded** where evidence is missing.

## Repository hygiene

- [ ] Keep local passwords out of commits. A pre-existing change is in tracked
  `include/config.h`; do not stage it accidentally. Prefer the ignored local override.
- [ ] Do not stage pre-existing `.factory/` or `security-findings.json`.
- [ ] Review diff and validation before any separately requested Conventional Commit.
