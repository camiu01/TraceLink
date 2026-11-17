# Changelog

Notable changes to TraceLink, most recent first. Dates mark when work was done in the workspace, not formal releases; there are no versioned releases yet.

Historical entries describe evidence available at that date, not current validation.
Use [PROJECT_STATUS.md](PROJECT_STATUS.md) for the current matrix and open limits.

## 2026-11-17 (unreleased)

- Fixed dashboard startup with an unsupported saved language and kept control
  request results separate from polled firmware status.
- Added six dependency-free dashboard regressions and a hardware-free CI workflow
  for the existing pinned firmware/native/MCP toolchain. No remote workflow run.
- Consolidated current verification status, historical evidence and open safety
  findings; replaced unsupported wiring/isolation claims with a conditional
  hardware guide, separate interface diagrams and preflight/pass criteria.
- Firmware TX, protocol, GPIO configuration and dependencies were not changed.
  No upload, physical serial access or bus operation.

## 2026-11-10

- Added optional local MCP stdio/HTTP bridge and explicit USB/COM backend, with
  host TX opt-in, no automatic write retries and simulated transport tests.
- Added TL1 USB requests and shared control validation; split acquisition/API/USB
  modules. Historical local tests/build passed; real-board MCP remained unverified.

## 2026-10-23

- Dashboard internationalization: Italian/English UI strings, header language toggle, browser-language default, choice persisted in localStorage, CSV headers follow the active language. Verified in a browser against mocked API responses (both languages, persistence, autoscroll and pause unaffected). Firmware-side status messages remain in Italian. JavaScript syntax check and firmware build passed. Not yet uploaded to the board.
- Documentation set standardized: README rewritten, `docs/` index and guides added, `CHANGELOG.md` and GPLv3 `LICENSE` added.

## 2026-10-22

- Dashboard autoscroll: follows new records, stops when scrolling up, resumes via checkbox. Checked in a browser against mocked API responses.
- Firmware built (RAM 20.0%, flash 62.5% of the default partition) and uploaded to an ESP32-D0WD-V3 over COM7 with hash verification; serial boot confirmed receive-only mode and the access point announcement. TX was not armed. The autoscroll build itself has not been uploaded.
- Local fallback AP password configuration changed in the working tree (uncommitted local configuration).

## 2026-10-06

- PlatformIO/Arduino project for ESP32 DevKit completed: `BusInterface` contract with `UartBus` and `CanBus`, 128-record ring history, strict HEX/number parsers, baud estimation.
- TX policy implemented: disabled at boot, explicit arm, 60 s idle auto-disarm, fail-closed partial enable, single-shot CAN with no delivery claim.
- `POST /api/control` validation with boot-token header; offline dashboard with cards, filters, pause, CSV export; speed scan UX; device profiles via `systems.json`; build-time asset embedding.
- Arduino C++11 build fixes (`BusHealth` constructor, explicit TWAI config types); firmware build green; host tests and JavaScript syntax check green.
- Documentation translated to English; wiring map documented; `STATO_PROGETTO.md` renamed to `PROJECT_STATUS.md`.
