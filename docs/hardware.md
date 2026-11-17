# Hardware and wiring

This is the single wiring guide. All diagrams are **logical, not physical or to scale**.
A firmware-compatible pin map is not proof that a connection is electrically safe.
Do not connect unidentified modules or external equipment using labels alone.

## Evidence and sources

- **Implemented:** GPIO assignments below match `include/config.h`, adapter construction
  in `src/TraceLink.cpp`, and UART/TWAI initialization in `src/bus_interface.cpp`.
- **Historically documented:** 22 October 2026 upload and boot on ESP32-D0WD-V3 rev 3.1.
  Firmware version and commit: **not recorded**. This is not validation of today's source.
- **User-reported:** WROOM-32 DevKit, UART0 free, 3.3 V RS232/RS485 wiring and detached
  external devices before that upload; on 17 November 2026 the user confirmed
  RS232 and RS485 working and supplied module listing descriptions and an RS485
  product illustration. Actual test date, RX versus TX coverage, firmware version,
  commit, source device and test pattern: **not recorded**.
- **Not measured:** actual connector continuity, rails, logic levels, termination,
  protective earth, isolation and physical RX/TX behavior of the current build.
  No installed-board photographs, board schematics or exact converter PCB revisions
  are in this repository. A commercial illustration is not a wiring photograph.

Official component reference consulted on 17 November 2026:
[TI SN65HVD230 product page](https://www.ti.com/product/SN65HVD230),
which identifies the **SN65HVD23x datasheet Rev. O, 4 April 2018**
([official HTML datasheet](https://www.ti.com/document-viewer/SN65HVD230/datasheet)).
The product page specifies a 3.0–3.6 V IC supply and up to 1 Mbit/s; its mode
description distinguishes driver-off standby from normal mode. It does **not**
identify a purchased breakout, its connector order, regulator, RS pin network or termination.
The PDF fetch was unavailable to the documentation reader and the HTML viewer
returned no datasheet body; only the product-page specifications were consulted.
No module schematic was verified.
No exact RS232/RS485 module datasheet or DevKit schematic was available.
Those connections remain conditional, not approved wiring instructions.

## Bill of materials and power

| Item | Required specification / role | Identification and power status |
| --- | --- | --- |
| Classic ESP32-WROOM-32 DevKit | Required; `esp32dev`, UART1/2 and TWAI, 3.3 V GPIO domain | User-reported WROOM-32; historic chip identification above. Exact DevKit vendor/revision and regulator budget unknown. GPIO16/17 map is not for WROVER, S3 or C3. |
| USB data cable | Required for board power, upload, log and TL1 API through UART0 bridge | Historic CP210x bridge; board-side USB connector type not recorded. Check physically; no assumed Micro-USB/USB-C pinout. |
| RS232 converter | Optional; genuine RS232 receiver, 3.3 V-compatible MCU I/O | User identifies a TTL/RS232 module advertised for 3.3–5 V, 3 mA, 300–120,000 bit/s, 43 × 25 mm. Exact IC/PCB and ratings not independently verified. |
| RS485 converter | Optional; half-duplex receiver with 3.3 V-compatible MCU I/O | User identifies the advertised isolated auto-direction 3.3/5 V module, up to 2 Mbit/s, onboard 120 ohms, 34 × 18 × 9 mm. Actual supply, circuit and isolation ratings unmeasured. |
| CAN converter | Optional; classic high-speed CAN physical layer with 3.3 V-compatible I/O | User identifies an SN65HVD230 module with slope control/thermal protection. Exact breakout revision, mode network and termination unknown; CAN operation not yet reported verified. |
| Bench source / peers | Known byte source per UART; two active CAN peers for a passive CAN monitor | Models, protocols and connectors not recorded. Required for meaningful verification. |
| Cables / adapters / terminators | Mating connectors, suitable differential twisted pair for RS485/CAN | Connector types, lengths, impedance, existing bias and termination must be identified before selection. No universal wire colors. |
| Isolation / isolated supply | If required by ground potential and installation safety assessment | Not established. A B0505XT-1WR3 label denotes a DC/DC converter, not a data transceiver or proof of whole-system isolation. Do not add it blindly. |

Use the board's identified USB power input for the initial USB-only trial.
Do not combine USB and another power source without the board's power-path schematic.
Do not assume the DevKit 3V3 regulator can supply every converter; record module current
requirements and rail measurements first. ESP32 GPIOs must not receive RS232 voltages,
the differential bus pair, 5 V logic or an external equipment supply.

## User-supplied module descriptions (17 November 2026)

These are **commercial claims supplied by the user**, not manufacturer datasheets
or measurements. No seller URL, manufacturer model or document revision was supplied.
The product illustration has not been copied into the repository; its visible
labels and relevant claims are transcribed here.

- **RS485:** listing advertises automatic direction, 3.3/5 V supply, up to 2 Mbit/s
  and an integrated 120-ohm resistor. It specifies a 2.54 mm TTL header and a
  5.08 mm KF301-compatible bus terminal footprint; these may require soldering.
  The illustration labels the bus terminals `A+`, `B+` and an earth-like symbol;
  its annotation says “Earth/vacant, not power negative”. TTL pads are labeled
  VIN/TX/RX/GND, but signal direction and PCB revision still need confirmation.
  Separate power and communication isolation are advertised, with no working
  voltage, test rating or certified barrier information provided.
- **RS232:** listing specifies 3.3–5.0 V operation, 5.5 V maximum, 3 mA,
  -40 to 80 °C and 300–120,000 bit/s; another sentence says 3–5 V. Do not
  treat these inconsistent bounds or the maximum as a recommended supply.
  It advertises ±15 kV ESD, TVS and overcurrent/overvoltage protection without a
  referenced test standard. TTL terminals are listed as VCC/GND/RXD/TXD and bus
  terminals as TXD/GND/RXD. Connector type/pitch and the exact IC are unspecified.
  The seller says to cross RXD/TXD on both sides; use the functional direction
  table below rather than assuming that wording identifies each module pad.
  The claimed TTL signal range follows VCC: a module powered at 5 V must not
  feed an unverified receiver output into an ESP32 input. The intended setup
  remains 3.3 V with compatible logic, not a blanket approval of 5 V operation.
- **CAN:** the user names SN65HVD230. It is the physical transceiver; the ESP32
  TWAI peripheral is the CAN controller. Slope/thermal features do not establish
  module isolation, protection ratings or a verified connector pinout.

Listing maxima do not extend firmware limits: serial rates remain 1200–115200,
fixed 8N1; classic CAN rates remain 125/250/500/1000 kbit/s.
The reported 120-ohm RS485 resistor must be accounted for before adding a tap:
confirm whether it is across the pair, fitted/enabled and removable. Do not
silently add another termination to an already terminated line.

## Point-to-point logic map

Names below describe **functions at the transceiver**, not arbitrary silkscreen RX/TX.
Only connect when the module schematic identifies those functions and compatible levels.

| ESP32 endpoint | Direction | Transceiver endpoint / condition |
| --- | --- | --- |
| GPIO16 / UART2 RX | Converter → ESP32 | RS232 receiver **logic output** (RO/RxOUT), not RS232 line input |
| GPIO17 / UART2 TX | ESP32 → converter | RS232 driver **logic input** (DI/TxIN); omit for receive-only wiring |
| GPIO26 / UART1 RX | Converter → ESP32 | RS485 receiver **logic output** (RO); reported module label TX must be checked |
| GPIO27 / UART1 TX | ESP32 → converter | RS485 driver **logic input** (DI); omit for receive-only only if module permits a defined disabled state |
| GPIO25 | ESP32 → converter control | Conventional manual-direction module: DE and active-low /RE, only if its schematic supports tying them together; LOW receives, HIGH transmits |
| GPIO32 / TWAI TX | ESP32 → converter | CAN driver logic input TXD (D on TI IC); never CANH |
| GPIO33 / TWAI RX | Converter → ESP32 | CAN receiver logic output RXD (R on TI IC); never CANL |
| GPIO1 / GPIO3 / UART0 | USB bridge ↔ ESP32 | Reserved for board bridge, boot, flashing, log and TL1; no external bus module |
| Identified 3V3 / logic GND | Supply / logic reference | Only compatible, identified TTL-side supply and reference; not automatically bus ground, shield or protective earth |

GPIO numbers are chip GPIOs, **not physical header positions**.
Identify the DevKit header orientation from its exact drawing.
`UartBus::begin()` configures both RX and TX even while software TX is disarmed:
TX idle levels and reset/startup transients are not a high-impedance guarantee.
Arming is a software permission, not electrical isolation or a safety interlock.

## RS232: observe one identified direction

```text
External device A line OUTPUT ----+----> existing device B line INPUT
                                 |
                                 +----> RS232 receiver LINE INPUT
                                        [level converter]
ESP32 GPIO16 UART2 RX <----------------- receiver LOGIC OUTPUT

Optional authorized transmit path, NOT part of the passive tap:
ESP32 GPIO17 UART2 TX -----------------> driver LOGIC INPUT
                                        [level converter]
Identified external line INPUT <-------- driver RS232 LINE OUTPUT
```

For capture, leave the converter's **RS232 driver line output disconnected**.
Do not join that output to A's output; two drivers on one line can contend.
The GPIO17 logic connection can also be omitted for a dedicated capture harness.
Confirm receiver loading, RS232 voltage range and signal-reference compatibility.

UART TTL and RS232 are different voltage domains; a crossed pair of wires is not a
level converter. Only A → B is observed; observing B → A needs another receiver and
firmware support. DTE/DCE roles, DB9 gender, pin numbers and straight/null-modem cable
requirements are **unknown**: no DB9 mapping is prescribed.
Identify actual endpoints and continuity before selecting a cable.

## RS485: shared half-duplex pair

```text
Known peer ===== identified differential pair ===== known peer
                         | short tap
                         +----> converter BUS pair (polarity to confirm)
                                [receiver / driver]
ESP32 GPIO26 UART1 RX <--------- RO, logic output
ESP32 GPIO27 UART1 TX ---------> DI, logic input (TX-capable harness only)
ESP32 GPIO25 -----------------> DE + /RE (manual-direction variant only)
```

Do not map `A`, `B`, `A+`, `B-` or the previously reported `A+/B+` labels by name
alone: confirm polarity against **both endpoint datasheets**. A/B conventions vary.
Record existing terminator and bias resistor values/jumpers before adding anything.
A passive tap normally must not add a third termination or an unplanned bias network;
the actual topology and component schematics determine the arrangement.

- **Manual direction:** only with compatible DE and /RE logic, GPIO25 LOW receives
  with the driver disabled; firmware raises it around armed sends.
- **Auto direction:** leave GPIO25 disconnected. The module must keep its driver
  inactive without MCU data. Omission of DI is allowed only if the module documents
  a defined inactive input; never assume a floating TX input is safe.
- **Capture-only:** require a documented receiver-only/driver-disabled configuration.
  Until the actual auto-direction circuit is identified, a guaranteed passive
  connection cannot be confirmed. Software disarming alone is insufficient.
- **TX-capable:** additionally verify direction/turnaround timing, bus ownership and
  collision rules with an isolated bench; it is not permission to transmit.

The previously reported isolation is unverified. Identify the data isolator,
isolated power circuit, creepage/clearance and rated barriers separately.
A product title or photograph alone proves none of these.

## CAN: controller and transceiver are separate

```text
Active peer 1 == CANH/CANL twisted-pair trunk == Active peer 2
     end termination                        end termination
                           |
                           +-- short tap --> converter CANH/CANL
                                             [CAN transceiver]
ESP32 GPIO33 TWAI RX <----------------------- RXD / R output
ESP32 GPIO32 TWAI TX -----------------------> TXD / D input
```

Use the exact transceiver/module schematic for supply, decoupling, mode/RS pin and
connector order. A bare SN65HVD230 does not provide galvanic isolation.
Firmware does not control a transceiver standby pin. TWAI listen-only at boot
suppresses controller transmission/ACKs, not all possible electrical failure modes.
A hardware driver-disabled receive path needs an identified transceiver mode circuit;
do not leave TXD floating to try to obtain it.

For a conventional high-speed CAN linear bench, identify the two physical ends
and their nominal 120-ohm terminations; do not add a third terminator at a tap.
Record any breakout resistor before attachment. Cable length, stubs, bitrate,
common-mode range and actual topology must be checked against component guidance.
Do not use an arbitrary DB9 or vehicle connector pinout.
Two active peers are needed to generate and acknowledge traffic when TraceLink is
listen-only. Arming puts TWAI in normal mode and permits ACKs even without a user
send. Single-shot TX queue success is not proof of arbitration success or delivery.
Classic CAN only; no CAN FD.

## References, grounds, shields and isolation

Logic GND on a non-isolated converter needs the reference required by its data sheet.
That does **not** authorize bonding an equipment ground, shield, chassis or protective
earth to the PC/USB ground. Determine common-mode voltage, existing bonds and the
installation's safety requirements first. Isolated modules have distinct logic/bus
domains only when both signal and power barriers are documented and not bypassed
by grounds, shields, instruments or USB.

The earlier blanket instruction to leave an earth terminal floating is withdrawn:
its function is not identified. Stop here until the module/installation schematic
establishes what that terminal is. No universal ground or shield connection is given.

## USB ownership

The board USB-UART connection powers the board and carries flashing, boot/log output
and TL1 API on UART0 at 115200 8N1. The MCP bridge lazily opens one explicitly named
port exclusively; close PlatformIO/Arduino monitors first. No port scan, fallback,
reconnect or automatic command retry is implemented. USB bridge/driver DTR/RTS behavior
can reset the ESP32; this has not been verified with MCP. A reset loses RAM history
and software arming. Do not treat opening a port as electrically inert.

## Before any future hardware trial (user-executed)

1. **Identify:** exact DevKit, IC markings, PCB revisions, connector views and pin
   numbers, cable continuity, target device/DTE/DCE roles, transceiver schematics.
2. **Unpowered:** detach all power/buses, verify routing and absence of output/output
   connections; identify termination, bias, DE//RE and isolation boundaries. Resistance
   measurements only on de-energized equipment with an appropriate procedure.
3. **Power only:** confirm allowed supply/input ranges and current budget from exact
   datasheets, then measure module rails and receiver logic levels before signals.
   Stop on any unknown voltage, shared reference or isolation rating.
4. **USB only:** no external bus or transceiver; separately authorize any upload,
   record firmware version/commit/build configuration and boot log. Keep TX disarmed.
5. **One interface:** known source and speed; collect a fixed, independently recorded
   pattern first in RX-only. Record raw expected/actual bytes, time interval, sequence
   gaps and driver counters. No data is not a passing result.
6. **Acquire before TX:** UART passes only with exact byte agreement for the chosen
   finite test pattern; CAN passes only with matching ID/flags/DLC/data against peer
   logs and explained losses. Baud estimates must match known sources across several
   patterns/rates, not just one sample. TX, idle timeout, fail-closed errors, CAN ACK,
   single-shot and return-to-listen-only need separate exact-command authorization.
7. **Archive:** dated photos (not proof of voltages), schematic revision, measured
   rails/references, cable/topology, source configuration, firmware commit/version,
   test duration, logs and pass/fail criteria. Use **not recorded** for missing fields.

Missing module data blocks the associated wiring instruction, not unrelated
software checks. No electrical measurements or physical actions were performed in
the 17 November software audit.
