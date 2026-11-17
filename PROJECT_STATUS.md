# TraceLink: status and next steps

## Current status (17 November 2026)

TraceLink is an experimental bench monitor for classic ESP32/WROOM-32: UART RS232,
UART RS485 and classic CAN, with a local dashboard and an optional HTTP/USB MCP bridge.
It is not a protocol decoder, CAN FD analyzer or production-validated instrument.

The source implements capture, bounded RAM history, explicit TX arming, command
queuing and baud estimation. Build/tests establish only the cases they execute.
The current source has **not** been uploaded or validated electrically in this audit.
Historic upload/boot evidence and user-reported capture are separated below.

Audit baseline: commit `b268c87e022b0f7ec7fe738016d86ebdbc7a12da` plus pre-existing
local configuration/untracked work and this audit's uncommitted changes.
Firmware version: **not recorded**. `include/config.h` contains a pre-existing local
change; it was not modified, staged or printed. `.factory/` and
`security-findings.json` were also pre-existing and were not changed.

## Verification matrix

| Feature / interface | Implemented | Automatic / simulated evidence | Real hardware evidence |
| --- | --- | --- | --- |
| Firmware build / assets | PlatformIO, pinned `espressif32@6.10.0`, embedded dashboard | Baseline build passed 17 Nov: RAM 66,704 B, flash 830,205 B. Final audit build recorded in the log below. | Historical 22 Oct upload/boot only; not the current build |
| History / parsers / USB framing | 128 RAM records, cursors/loss, bounded fields, TL1 request CRC/IDs | Both native C++ suites passed 17 Nov; no drivers/scheduler exercised | Not independently verified |
| RS232 RX | UART2, GPIO16 input / GPIO17 output, 9600 8N1 default | Parser/history tests; not an electrical RX test | Working capture reported by user; date/version/commit/source/pattern **not recorded** |
| RS485 RX / direction | UART1, GPIO26 input / GPIO27 output, optional GPIO25 DE//RE | Source inspection only for physical behavior | Working reported by user on 17 Nov; actual test date/version/RX-TX coverage not recorded; isolation unmeasured |
| CAN RX / TX | TWAI GPIO33 input / GPIO32 output; listen-only boot; single-shot queue | Validation tests, not driver/ACK/bus-off tests | User identifies SN65HVD230 module; operation not verified, exact PCB/mode/termination unknown |
| TX arming / idle / partial errors | Acquisition task executes single-slot queued controls | Admission/parser and bridge-gate tests only, not FreeRTOS execution/physical interlocks | Not verified; review items requiring approval remain open |
| Baud probing | Serial pulse heuristic, CAN listen-only candidate search | Histogram unit tests only | Known-source accuracy/scan loss tests not done |
| Dashboard | Offline assets, i18n, pause, autoscroll, text rendering, CSV | Six script-level regressions and isolated Chromium with simulated fetch, 17 Nov; historical browser checks below | Full dashboard/Wi-Fi authentication not independently verified |
| MCP HTTP / stdio | Host TX opt-in, per-call confirmation field, no automatic command retries | 38 tests with local HTTP peers and actual MCP clients, 17 Nov | No ESP32 MCP test |
| MCP USB | Explicit named exclusive serial stream, no discovery/fallback/reconnect | Mock OS binding and native frame parser tests, 17 Nov; not full ESP32 execution | Never exercised with the current firmware |
| CI | Local-only build/native/MCP/dashboard workflow prepared | Commands checked locally; GitHub runner not executed | No hardware required or used |

`confirm:true` is not evidence of human approval. The MCP client must enforce it.
HTTP/USB 202 means queued, not executed or delivered. Repeated requests are not
deduplicated. CAN normal mode sends ACKs even without user sends.

## Open issues and next steps

See [audit findings](docs/audit-2026-11-17.md) for confirmed defects, source-level
risks, evidence and changes deliberately withheld pending approval.

1. Resolve the TX timeout ordering, CAN driver-failure handling and strict CAN flag
   validation design before changing safety behavior. No such change was made.
2. Identify exact DevKit/transceiver PCBs, supplies, connector pinouts, RS485
   direction circuit, termination/bias and signal/power isolation separately.
   Follow the conditional [hardware guide](docs/hardware.md), not old blanket
   assumptions about module RX/TX, 3.3 V supply, grounds or floating earth.
3. Authorize future upload and RX-only tests separately, one interface at a time.
   First collect known-source bytes and physical evidence; no hardware action
   is authorized by this document.
4. Test acquisition scheduling, bus-off, real UART overflow, USB reset/throughput,
   60 s disarm, partial driver errors and sustained concurrent HTTP/USB load.
   Native/MCP mocks do not establish these properties.
5. Verify storage-disabled browsers, mobile layout and long-session pause/export
   expectations. Pause freezes rendering while retained data continues updating;
   CSV uses the current filtered retained records, not a frozen visual snapshot.

No automatic retries, transport fallbacks, telemetry, new runtime dependencies,
GPIO changes, uploads or real serial/bus operations were added by the audit.
No commit, push or remote workflow was performed.

At audit handoff the user chose to keep TX/compatibility findings open and deliver
this audit. The user subsequently supplied commercial module descriptions and an
RS485 illustration; exact PCB schematics, electrical measurements and installed
wiring photographs remain missing.

## Chronological verification log

### 17 November 2026: user hardware report and publication authorization

- User confirmed RS232 and RS485 working. Actual test date, firmware version,
  commit, source device, baud and RX/TX coverage: **not recorded**.
- User identifies an SN65HVD230 CAN transceiver module, an advertised isolated
  auto-direction 3.3/5 V RS485 module with onboard 120 ohms, and a 3.3–5 V
  TTL/RS232 converter. Listing specifications and illustration labels are recorded
  in docs/hardware.md as user-supplied claims, not verified electrical ratings.
- User authorized commit and push of the audit. Local credentials and pre-existing
  untracked work remain excluded. The publication commit uses `[skip ci]` to
  preserve the earlier instruction not to activate remote workflows.

Historical entries retain the reports made at the time, including stronger safety
wording later qualified by the 17 November audit. They are not current guarantees.

### 17 November 2026: local software audit

- Baseline: PlatformIO Core 6.1.19 build passed (RAM 66,704 B; flash 830,205 B).
  Both C++11 native suites passed with `-Wall -Wextra -Werror`.
  MCP `npm test`: 38/38 passed on Node.js 24.13.1 / npm 9.2.0.
- Reproduced dashboard startup failure with an unsupported persisted language
  (`TypeError`), and loss of a control error on the next unchanged status poll.
  Three new assertions failed before the fixes; two unrelated checks passed.
- Fixed language fallback and separated request results from firmware status.
  Six dashboard tests now pass, including pause/safety refresh, reboot cursor reset,
  text-only rendering, autoscroll position and CSV formula/quote protection.
- Isolated Chromium, simulated fetch only: startup, text rendering, persistent
  403 result, CSV contents, paused safety refresh, reboot, offline disabling and
  exactly one control POST all passed. This was not a device-served page or a
  comprehensive browser/layout test. The audit tab was closed.
- Added a minimal GitHub Actions workflow; no remote run activated.
- Clean isolated MCP installation (`npm ci --no-audit --no-fund`) and its test
  suite passed 38/38; the temporary copy was removed without replacing local
  node_modules. Final firmware build passed: RAM **66,704 B**, flash **830,413 B**.
- Replaced unsupported electrical claims with conditional diagrams and explicit
  unknowns. Consulted TI SN65HVD230 product page identifying Rev. O datasheet;
  exact converter schematics remain unavailable.
- No physical port was opened, no upload or electrical measurement occurred.
- `postman status` is unsupported by installed CLI 1.75.0. The standalone
  simulation was not rerun; MCP tests used the existing mock over local sockets.
### Historical verification: 10 November 2026 (USB / COM transport)

- Added an alternative MCP device connection through one explicit USB serial
  port (`TRACELINK_SERIAL_PORT`, for example an identified `COM7`), at 115200
  baud, 8N1. Remove `TRACELINK_URL` when using USB. Both stdio and local HTTP
  MCP clients can use this backend. Port access is lazy and exclusive; there
  is no port probing, reconnect, Wi-Fi fallback or automatic write retry.
- The firmware now accepts bounded TL1 frames with request IDs, CRC32 and form
  fields on USB/UART0. Human-readable capture logs remain available and a USB
  output mutex prevents interleaving with API replies. GPIO assignments and
  UART0 baud are unchanged. USB mode does not require PC Wi-Fi.
- HTTP and USB share command/payload validation, boot-token checks, admission
  rules and the acquisition-task queue. Embedded NUL form fields are explicitly
  rejected instead of allowing C-string truncation; send binary NUL as HEX 00.
  TX defaults, CAN ACK warnings, idle timeout and fail-closed execution remain.
- Split `src/TraceLink.cpp` into acquisition/setup (275 lines),
  `src/api.cpp` and `src/usb_console.cpp`, each below 400 lines.
- `npm test` in `mcp/`: **38 tests passed** on Node.js 24.13.1. The serial tests
  use the real stream with an OS mock binding, fragmented replies and a local
  HTTP-derived wire peer, not actual ESP32 firmware execution. They cover
  maximum-sized event batches, the 64 KiB receive boundary, TX gates, errors,
  ambiguous writes, stale IDs, explicit connection selection and port cleanup.
- Existing `history_test.cpp` and new `control_usb_test.cpp` passed with
  `-std=c++11 -Wall -Wextra -Werror`. The latter checks shared validation,
  admission states, CRC against a Python zlib vector, framing limits, malformed
  forms, stale/oversized input and timestamp wraparound.
- Final `pio run` passed: RAM **66,704 bytes (20.4%)**, flash **830,205 bytes
  (63.3%)**. The user's pre-existing `include/config.h` change was preserved.
- Setup: `docs/mcp.md`, `mcp/client.serial.example.json`; wire format:
  `docs/api.md`. Old firmware cannot answer USB MCP commands, so a later,
  explicitly authorized upload is required.
- No real COM port was opened, no firmware was uploaded, and no physical bus
  transmission or USB reset behavior was tested. Opening a USB-UART port may
  reset the board depending on its driver. No servers/tests were left running.

### Historical verification: 10 November 2026 (Windows, MCP bridge)

- Added `mcp/`, a separate Node.js 22+ MCP bridge using the official SDK. Stdio
  and bearer-protected, loopback-only Streamable HTTP expose status, paginated
  RX/TX reads, arm/disarm and single sends through the existing firmware API.
- TX is blocked by default (`TRACELINK_ALLOW_TX=1` is an explicit host opt-in).
  Arming/sending require `confirm:true`, but actual human approval must be
  enforced by the MCP client. Firmware arming, idle timeout and validation remain
  in force. CAN arming enables ACKs. Controls never automatically retry.
- `npm test` in `mcp/`: 19 tests passed on Node.js 24.13.1, including real SDK
  clients over stdio/HTTP, payload limits, pagination, reboot/loss handling,
  Host/Origin/token checks, overlapping controls and uncertain write outcomes.
- Postman CLI 1.75.0 local simulation smoke test passed through the actual stdio
  entry point: discovery, pagination, disarm/arm, send, captured TX and final
  disarm. The simulation log confirmed HTTP GETs and four control POSTs (202).
  `postman status` is not supported by this installed CLI; no cloud resources
  were created or published. The mock is derived from local firmware/API docs,
  not a fetched team resource.
- Existing native C++ tests passed with `-std=c++11 -Wall -Wextra -Werror`.
  `pio run` passed: RAM 65,656 bytes, flash 828,621 bytes. No firmware source was
  changed; the pre-existing local change in `include/config.h` was preserved.
- See `docs/mcp.md` and `mcp/client.example.json` for setup. No client app was
  configured automatically, no upload or hardware TX was performed, and real
  ESP32 MCP connectivity is unverified. HTTP is local-only, not cloud hosting.

### Historical verification: 23 October 2026 (Windows)

- Dashboard i18n (Italian/English toggle, browser-language default, localStorage persistence) verified in a browser with mocked API traffic in both languages. JavaScript syntax check and `pio run` build passed. Not yet uploaded to the board. Firmware-side messages remain Italian.
- Documentation standardized: README rewritten, `docs/` guides (usage, architecture, API, hardware, testing, troubleshooting), `CHANGELOG.md`, and GPLv3 `LICENSE` added. `include/config.h` carries an uncommitted local password change and stays out of version control.

### Historical verification: 22 October 2026 (Windows)

- Dashboard autoscroll added: follows new records by default, stops when scrolling up, and resumes with the checkbox. Verified in an isolated browser with mocked API traffic (follow, suspension, reading-position retention, resume, pause/resume, clear). JavaScript syntax, native C++ tests, and firmware build passed (flash: 819,229 bytes). This change has not been uploaded. Baud scanning is unchanged: the user has not yet tried the manual scan button.
- Subsequent user-requested rebuild/upload with the updated configuration also passed on COM7 (flash: 818,589 bytes). Serial boot again reported RX-only and the access point; Wi-Fi authentication was not tested.
- User confirmed ESP32-WROOM-32 / classic DevKit, the documented 3.3 V RS232/RS485 wiring, UART0 free, and external bus devices disconnected before upload.
- PlatformIO Core 6.1.19: firmware build passed with `espressif32@6.10.0` and Arduino 2.0.17. RAM: 65,656 bytes (20.0%); flash: 818,605 bytes (62.5%).
- Upload on COM7 passed, including flash hash verification. Detected chip: ESP32-D0WD-V3, revision 3.1. No full erase or eFuse changes.
- A bounded 115200-baud serial capture verified boot, `RX only al boot`, and the reported access point `TraceLink-ESP32` at `http://192.168.4.1`. TX was not enabled.
- Initial build was blocked by a full C: drive; after the user freed space, build and upload succeeded.
- Wi-Fi connection and real bus RX/TX remain independently unverified. The user subsequently reported working capture. Native and simulated-browser checks for the newer autoscroll change are listed above.
- This update supersedes the older no-upload/no-detected-board statements below. No build or monitor process was left running.

### 6 October 2026: historical build and blocked hardware access

- Historical environment: project `.venv/`, PlatformIO downloads completed.
  Arduino C++11 fixes included the BusHealth constructor and explicit TWAI config types.
- `.venv/bin/pio run` succeeded: RAM 20.0%, flash 62.3%, firmware.bin/ELF and web
  header generated. Firmware version and commit: **not recorded**.
- Native tests and extracted-script JavaScript syntax passed.
- ASan/UBSan was not run: linker lacked `/usr/lib64/libasan.so.8.0.0`.
- That host showed no ttyUSB/ttyACM/serial-by-id or USB serial bridge; no upload,
  browser or physical bus verification was recorded **at that time**.
  This does not negate the later 22 October upload or simulated browser checks.
- Older unchecked first-upload/browser checklists are superseded by the matrix
  above. Remaining work is in TODO.md and the hardware checklist.
