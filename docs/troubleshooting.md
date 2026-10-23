# Troubleshooting

Observed issues and diagnostics. Causes marked as uncertain have not been root-caused yet; none of these entries are confirmed firmware bugs unless stated.

## Host and toolchain

| Symptom | Checks and boundary |
| --- | --- |
| `pio run` fails with `No space left on device` while downloading the platform | Free disk space; the full ESP32 platform and toolchain need roughly 2 GB. Observed once on a full C: drive; the build succeeded after space was freed. |
| Board not visible as a serial port | Check the USB cable (data, not charge-only) and the bridge driver. On this project the board appears as a Silicon Labs CP210x; a Windows Code 10 on that device was resolved by re-plugging. A port that does not appear in the system port list cannot be flashed. |
| `pio run --target upload` connects to the wrong port | Pass the port explicitly: `pio run --target upload --upload-port COMx`. Never flash to an automatically selected port without identifying the board. |

## Firmware and dashboard

| Symptom | Checks and boundary |
| --- | --- |
| Dashboard shows `ESP32 non raggiungibile` | Confirm the device is connected to the `TraceLink-ESP32` network and that the OS did not drop it for having no Internet. The page exists only at `http://192.168.4.1`. Retry is automatic. |
| Control request answers 403 | The ESP32 rebooted and the boot token changed. Reload the page. |
| Control request answers 409 | Conflicting state: scanning is in progress, TX is armed (for scan), TX is disarmed (for send), or another command is being executed. Wait or adjust the state and retry once. |
| Control request answers 400 | The response names the rejected field: malformed HEX (use pairs like `01 03 FF`), unsupported rate, CAN identifier out of range, RTR with a payload, empty serial data, or input over 256 characters. |
| Scan reports `non rilevata` | Detection needs live traffic. The serial estimate is a pulse-timing heuristic on an 8N1 assumption; data patterns can also produce a wrong rate. Confirm against device documentation before applying. |
| Counters reset during a scan | Expected: the CAN driver is reinstalled per candidate rate, which restarts its missed/overrun/error counters, and frames can be lost during reconfiguration. |
| Old records disappear | The history holds 128 records in RAM and overwrites the oldest; the browser keeps 500. Overwrites and client-side gaps are shown on the dashboard. Nothing persists across reboots. |
| Dashboard text is in the wrong language | Use the IT/EN toggle in the header; the choice persists in the browser. Firmware-provided status messages remain in Italian regardless of the dashboard language. |

## Hardware

| Symptom | Checks and boundary |
| --- | --- |
| No RS232 data from a tapped line | Confirm the tap is on the module's RS232-side RX (not the TTL side), the line actually carries traffic, and the configured speed matches. RS232 levels must never reach a GPIO. |
| Garbled serial data | Likely a speed or ground problem. Verify the applied rate, the shared signal ground (after compatibility checks), and 3.3 V module supply. Parity is fixed to 8N1; devices using other formats are not supported. |
| CAN stays `non disponibile` | Check the serial boot log for the TWAI error name, the transceiver wiring on GPIO32/33, and the transceiver supply. A silent or unterminated bus can also yield no detection. |

Do not use full flash erase, eFuse changes, or overclocking as recovery steps; they are outside the project's safety rules. Destructive recovery and disabling protections are not documented fixes.
