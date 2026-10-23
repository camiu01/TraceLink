# Changelog

Notable changes to TraceLink, most recent first. Dates mark when work was done in the workspace, not formal releases; there are no versioned releases yet.

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
