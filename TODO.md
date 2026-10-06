# TODO

Development task management for the TraceLink ESP32 multi-bus monitor.

## Done

- [x] PlatformIO Arduino project for ESP32 DevKit (`espressif32@6.10.0`) with `TraceLink.cpp` acquisition + HTTP + USB tasks #infra #firmware
- [x] `BusInterface` contract with `UartBus` (UART + optional DE/RE) and `CanBus` (TWAI listen-only/normal) adapters #firmware #modularity
- [x] 128-record ring history with sequences, per-source counters and HEX formatting (`capture.h`) #firmware
- [x] Strict HEX parser, overflow-checked numbers, serial/CAN rate tables, pulse-histogram baud estimate (`commands.h`) #firmware #tests
- [x] TX disabled at boot, explicit arm/disarm, 60 s idle auto-disarm, fail-closed partial enable, single-shot CAN with no delivery claim #safety #firmware
- [x] `POST /api/control` validation (HEX/text, CAN id/DLC/RTR, boot-token header, no CORS, no auto-resend) #firmware #security
- [x] Offline dashboard: dynamic cards, filters, RX/TX view, pause/clear, CSV export with formula neutralization, text-only bus rendering #ux #web
- [x] Speed scan UX: ~5 s listen, serial estimates for confirmation, detected CAN rate applied #ux #web
- [x] Profile system (`systems.json` + provider endpoint): loading fills fields only, never arms or sends #ux #data
- [x] Asset embedding (`embed_web.py` packs HTML + validated profiles into `web_page.h`, no filesystem upload) #infra
- [x] Host-side test suite (`history_test.cpp`: history, HEX, parsers, rates, baud estimate) #tests
- [x] Arduino C++11 build fixes: `BusHealth` constructor, explicit TWAI config types (`bus_interface.h`, `bus_interface.cpp`) #firmware
- [x] `.venv/bin/pio run` SUCCESS (RAM 20.0%, Flash 62.3%), `firmware.bin`/`firmware.elf` + `web_page.h` generated #tests
- [x] Native tests green (`-std=c++11 -Wall -Wextra -Werror`) and inline-script `node --check` green #tests
- [x] Modules identified: isolated auto-direction TTL-RS485 + blue SP3232 TTL-RS232, 3.3 V TTL wiring map, UART0 kept free #docs #hardware
- [x] English documentation pass: README, PROJECT_STATUS, agent instructions; Doxygen `@file`/`@brief` + JSDoc; tabs indentation #docs #style
- [x] `STATO_PROGETTO.md` renamed to `PROJECT_STATUS.md` via `mv`, references updated #docs #hygiene

### Docs & style (this pass)

- [x] README rewrite in Haguruma style (tagline, What's new, Features, Bus engine, Wiring, Run it, Structure, Safety) #docs
- [x] TODO.md in Haguruma style (Done/Pending with milestone sections and tags) #docs

## Pending

### Milestone 1 — Confirm wiring and upload safely

Board identification and first USB-only boot. No upload without an identified port, confirmed 3.3 V wiring and explicit authorization.

- [ ] Confirm physical rewiring: UART0 free, RS232 crossed to GPIO16/17, RS485 to GPIO26/27, GPIO25 unconnected, 3.3 V TTL supplies #hardware
- [ ] Identify ESP32 model, USB port and data cable on the host machine; record `pio device list` evidence #hardware
- [ ] Get explicit upload authorization; flash with board port only (no full erase, no eFuse, no overclock) #hardware
- [ ] Verify boot log, Wi-Fi AP and page load with transceivers detached and TX never armed #hardware #tests

### Milestone 2 — Web page verification (simulated APIs, no bus TX)

- [ ] Desktop/mobile layout, offline banner, ESP32 reboot handling, filters, pause, CSV #ux #web
- [ ] Manual speeds, scan flow, profiles, standard/extended/RTR CAN fields #ux #web
- [ ] Errors and TX state visible with the view paused; safety state refreshes independently #ux #safety
- [ ] Profile-load and HTTP-result messages not covered by status polling #ux #web
- [ ] Invalid profile cannot become an automatic send #ux #security

### Milestone 3 — Safety and modularity audit

- [ ] TX arm/disarm, timeout, partial-enable failure, concurrent requests #safety #firmware
- [ ] CAN returns to listen-only on disarm; document queued-vs-delivered CAN semantics #safety #docs
- [ ] Limits enforced on serial length, CAN id/DLC/payload in firmware #safety #firmware
- [ ] Bus-off, CAN errors and driver restart behavior #firmware
- [ ] HTTP + Wi-Fi limits assessed for sensitive/production use #security #docs
- [ ] New-protocol extension path tested (adapter + source + validation + UI + tests) #modularity

### Milestone 4 — Docs and convention cleanup

- [ ] Split `src/TraceLink.cpp` (486 lines, over the 400-line limit); HTTP handlers to a separate unit #style #firmware
- [ ] Keep every function under 50 executable lines; guard clauses over nesting #style
- [ ] Replace example profile commands with the user's real documented device commands #docs #data
- [ ] Add `LICENSE` before any public release #docs

### Milestone 5 — Safe hardware trials, one interface at a time

- [ ] RS232 against a known source at the configured speed; compare HEX bytes #hardware #tests
- [ ] RS485 on a live master/slave line; RX first, TX only with confirmed device + exact command #hardware #safety
- [ ] CAN on a two-node terminated bench; check ID/DLC/data and loss counters under load #hardware #tests
- [ ] Record real loss behavior; absence of data never counts as proof of wiring #hardware #docs

### Conventions & hygiene

- [ ] No committed secrets (`local_config.h` stays gitignored); versions pinned, no `latest` tags #hygiene #security
- [ ] Conventional Commits in English; no push without request; no force-push #hygiene
- [ ] Failing validation 3x in a row stops the pipeline and waits for guidance #hygiene
