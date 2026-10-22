# TraceLink: status and next steps

## Verified update: 22 October 2026 (Windows)

- Dashboard autoscroll added: follows new records by default, stops when scrolling up, and resumes with the checkbox. Verified in an isolated browser with mocked API traffic (follow, suspension, reading-position retention, resume, pause/resume, clear). JavaScript syntax, native C++ tests, and firmware build passed (flash: 819,229 bytes). This change has not been uploaded. Baud scanning is unchanged: the user has not yet tried the manual scan button.
- Subsequent user-requested rebuild/upload with the updated configuration also passed on COM7 (flash: 818,589 bytes). Serial boot again reported RX-only and the access point; Wi-Fi authentication was not tested.
- User confirmed ESP32-WROOM-32 / classic DevKit, the documented 3.3 V RS232/RS485 wiring, UART0 free, and external bus devices disconnected before upload.
- PlatformIO Core 6.1.19: firmware build passed with `espressif32@6.10.0` and Arduino 2.0.17. RAM: 65,656 bytes (20.0%); flash: 818,605 bytes (62.5%).
- Upload on COM7 passed, including flash hash verification. Detected chip: ESP32-D0WD-V3, revision 3.1. No full erase or eFuse changes.
- A bounded 115200-baud serial capture verified boot, `RX only al boot`, and the reported access point `TraceLink-ESP32` at `http://192.168.4.1`. TX was not enabled.
- Initial build was blocked by a full C: drive; after the user freed space, build and upload succeeded.
- Wi-Fi connection and real bus RX/TX remain independently unverified. The user subsequently reported working capture. Native and simulated-browser checks for the newer autoscroll change are listed above.
- This update supersedes the older no-upload/no-detected-board statements below. No build or monitor process was left running.

Updated on 6 October 2026. Programming resumed at the user's request: firmware build succeeds, upload and hardware tests still require authorization. No ESP32 was detected on this host USB (no `ttyUSB`/`ttyACM`/`serial/by-id`, no serial bridge in `lsusb`).

## Goal

Build a PlatformIO project with the Arduino framework for classic ESP32 / ESP32-WROOM-32 that:

- reads RS232 on TX2/RX2, RS485, and CAN bus;
- shows traffic and counters on the USB monitor and the Wi-Fi web page;
- probes unknown speeds and sets them manually;
- sends commands from the web page only after explicit enabling;
- adds devices and commands through profiles, and new interfaces through modules.

The project name is **TraceLink**.

## What is done

The firmware **builds successfully** (`firmware.bin` ~805 kB, `firmware.elf` generated). It has **not been uploaded or verified on ESP32 yet**.

| Area | Current state |
| --- | --- |
| Project | `platformio.ini`: ESP32 DevKit, Arduino, platform `espressif32@6.10.0`. |
| Configuration | `include/config.h`: GPIOs, initial speeds, buffers, Wi-Fi, and 60-second TX window. Password override in `include/local_config.h`, excluded from Git. `rs485De` documented as unconnected for auto-direction modules. |
| Bus modules | `include/bus_interface.h` and `src/bus_interface.cpp`: common contract plus UART/RS485/CAN adapters. Fixed C++11 aggregate init (`BusHealth` constructor) and explicit TWAI config types. |
| Acquisition | `src/TraceLink.cpp`: acquisition task, history, USB task, and HTTP server. |
| History | `include/capture.h`: 128-record ring buffer, sequences, counters, and HEX conversion. |
| Speed control | `include/commands.h` and runtime: serial estimate from RX pulses, CAN search, and manual speed change. To verify on hardware. |
| Sending | `POST /api/control` endpoint, HEX/text/CAN-ID validation, TX disabled at boot, explicit arming, auto-disarm after 60 seconds without sends. To verify. |
| Dashboard | `web/index.html`: dynamic interfaces, filters, RX/TX view, pause, CSV, speed control, sending, and profile selection. Not yet tried in a browser. |
| Profiles | `web/systems.json`: one example profile, **not a validated real protocol**. Loading a profile fills the fields, does not arm TX, and does not send. |
| Assets | `scripts/embed_web.py`: embeds HTML and profiles into the firmware; no separate filesystem upload. `web_page.h` regenerates during build. |
| Initial docs | `README.md`: Haguruma-style rewrite (tagline, What's new, Features, Bus engine, Wiring, Run it, Structure). Full checklist in `TODO.md`. |
| Local tests | `test/native/history_test.cpp`: history, HEX, numeric parser, allowed speeds, and baud estimation. |

### Current settings

| Interface | ESP32 GPIO | Initial speed |
| --- | --- | --- |
| RS232 / UART2 | RX2 = 16, TX2 = 17 | 9600 baud, 8N1 |
| RS485 / UART1 | RX = 26, TX = 27, DE+/RE = 25 (unconnected on auto-direction module) | 9600 baud, 8N1 |
| CAN / TWAI | RX = 33, TX = 32 | 500 kbit/s, listen-only at boot |
| USB monitor | Board UART0 (GPIO1/GPIO3, must stay free) | 115200 baud |

Wi-Fi: access point `TraceLink-ESP32`, page at `http://192.168.4.1`.

The fallback password `change-this-password` is public and must be replaced before use.

## Hardware on hand and pending choices

- RS232: blue TTL/UART-RS232 module with Sipex chip, 3.3-5 V. To run at 3.3 V with `VCC -> 3V3`, crossed `RXD/TXD` to GPIO17/GPIO16.
- RS485: isolated TTL-to-RS485 auto-direction module (VIN, TX, RX, GND; A+, B+, floating earth). To run at 3.3 V with module `TX -> GPIO26`, module `RX <- GPIO27`, GPIO25 left unconnected.
- The user previously wired 232 to RX0/TX0 and RS485 to TX2/RX2. Corrected on paper to the table above; physical rewiring still to confirm with the user.
- The B0505XT-1WR3 (5 V to 5 V DC/DC) is not needed: the RS485 module already integrates isolation.
- For CAN, an SN65HVD230 3.3 V module using the built-in TWAI controller was proposed. Not yet purchased or connected.
- The user reports an attached/online ESP, but this host shows no identifiable board, no `ttyUSB`/`ttyACM` ports, and no `/dev/serial/by-id`.

**No upload or bus transmission has been performed on real hardware.** Before doing so, identify the board and confirm wiring, voltages, and what is attached to the GPIOs.

## Verifications performed

- PlatformIO in project `.venv/`; ESP32 platform and toolchain downloads completed on 6 October 2026.
- Fixed build errors from Arduino C++11: `BusHealth` brace init and `auto` deduction on TWAI macros (`twai_general_config_t`, `twai_timing_config_t`, `twai_filter_config_t`).
- `.venv/bin/pio run`: **SUCCESS** (RAM 20.0%, Flash 62.3%), `firmware.bin`/`firmware.elf` generated plus `web_page.h`.
- Local C++ tests with `-std=c++11 -Wall -Wextra -Werror`: passed after the fix.
- JavaScript check by extracting the inline `<script>` and running `node --check`: passed.
- Attempt with AddressSanitizer/UndefinedBehaviorSanitizer: not run because the linker missed `/usr/lib64/libasan.so.8.0.0`. Tests ran without sanitizers.
- No dashboard check in a browser, no electrical checks, and no real RS232/RS485/CAN trials.

Do not treat these checks as certification of the latest version.

## What is missing, in order

The full checklist lives in [TODO.md](TODO.md); this section summarizes it.

### 1. Confirm wiring and upload safely

- [x] Read `agent.md` and this file before resuming.
- [x] Complete PlatformIO downloads and build.
- [x] Fix Arduino/TWAI API incompatibilities and project warnings.
- [x] Verify HTML/JSON header generation.
- [x] Rerun C++ tests and JavaScript check on the latest version.
- [ ] Confirm physical rewiring to the table above (UART0 free, 3.3 V supplies).
- [ ] Identify ESP32 model, USB port, and cable; get upload authorization; no full erase or eFuse changes.

Commands from the project folder:

```sh
.venv/bin/pio run
g++ -std=c++11 -Wall -Wextra -Werror -Iinclude test/native/history_test.cpp -o /tmp/tracelink-history-test
/tmp/tracelink-history-test
```

### 2. Complete and verify the web page

- [ ] Try the page with simulated APIs, without sending anything on real buses.
- [ ] Check desktop/mobile, offline state, ESP32 reboot, filters, pause, and CSV.
- [ ] Try manual speeds, scanning, profiles, and standard/extended/RTR CAN fields.
- [ ] Check errors and TX state even with the view paused.
- [ ] Verify profile-load/HTTP-result messages are not immediately covered by status polling.
- [ ] Verify an invalid profile cannot become an automatic send.

### 3. Verify safety and modularity

- [ ] Check TX arm/disarm, timeout, partial errors, and concurrent requests.
- [ ] Verify CAN really returns to listen-only when TX is disabled.
- [ ] Check limits on serial data, CAN ID, DLC, and payload.
- [ ] Document the difference between a queued CAN frame and an actually delivered frame: today's `twai_transmit()` success does not confirm delivery.
- [ ] Verify bus-off behavior, CAN errors, and driver restarts.
- [ ] Assess that HTTP and Wi-Fi access are not enough for sensitive or production plants.
- [ ] Refine extensions: the dashboard reads dynamic descriptors, but speeds, payloads, and some CAN/RS232/RS485 parts remain specific. New protocols need code and tests.

### 4. Finish documentation and code-convention cleanup

- [x] Translate all documentation to English.
- [ ] Verify English Doxygen/JSDoc coverage on every file and public function.
- [ ] Normalize indentation to tabs across C++/headers and the inline script.
- [ ] Split `src/TraceLink.cpp` (486 lines, over the 400-line file limit) and keep functions under 50 executable lines.
- [ ] Replace example commands with the user's actually documented device commands.

### 5. Identify the ESP32 and run safe hardware tests

- [ ] Recheck USB and serial port, board model, and data cable.
- [ ] Ask for wiring confirmation before flashing or changing GPIO configuration.
- [ ] For the first trial prefer a board connected **via USB only**, without transceivers or external buses.
- [ ] Get upload authorization; no full erase or eFuse modification.
- [ ] Verify boot, serial, and Wi-Fi without arming TX.
- [ ] Connect and test one interface at a time, with correct voltages.
- [ ] Use a known source to verify serial estimates at several speeds.
- [ ] Test CAN on a bench with at least two active nodes and correct termination.
- [ ] Run TX trials only after explicit user confirmation of device, wiring, speed, and exact command.
- [ ] Measure losses and under-load behavior; record real results.

## Limits to remember

- No speed detection is possible on a line without traffic.
- Serial probing is a **pulse-timing estimate**, not guaranteed baud-rate, parity, or protocol recognition. It never applies the estimate automatically.
- CAN search tries 500/250/125/1000 kbit/s for about 1.2 seconds each; it stops after three received frames and keeps the detected speed. Without results it restores the initial speed.
- During CAN probing, speed and driver change temporarily: frames can be lost and driver counters can restart.
- GPIO sampling, speed changes, and UART sending can affect simultaneous acquisition.
- The default serial format is 8N1; there is no parity scan.
- CAN FD is not supported.
- No Modbus decoding or automatic CRC generation: commands are user-supplied bytes.
- History is in RAM, without persistent storage. UART overflows are not counted.
- A new profile adds commands of a device on existing interfaces; it does not create new physical ports.

## Resume point

Build is green; do not rebuild from scratch. Next: confirm the rewiring table, identify the ESP32 USB port on the machine that physically hosts it, get explicit upload authorization, then boot with USB only and no TX. Update this document with evidence and any errors. Do not start background builds, servers, or tests when suspending.
