# Usage guide

[← README](../README.md) · [Docs index](README.md)

This guide covers the everyday workflow of TraceLink once the firmware is flashed. Paths are relative to the repository root. The dashboard is bilingual: the header toggle switches between Italian and English, the choice persists in the browser (localStorage), and the initial language follows the browser language. Italian labels are quoted below. Status messages returned by the firmware (`message` field, control responses) remain in Italian.

## Startup

1. Power the board over USB. The boot log (115200 baud on the USB monitor) prints `TraceLink | RX only al boot | TX da abilitare esplicitamente` and then `Wi-Fi: TraceLink-ESP32 | http://192.168.4.1`.
2. Connect a phone or computer to the `TraceLink-ESP32` Wi-Fi network. Keep the connection when the operating system warns that the network has no Internet access.
3. Open `http://192.168.4.1`. The dashboard loads without any external network.

The header badge shows the connection state: `Connesso · acquisizione attiva`, `Connesso · vista in pausa`, or `ESP32 non raggiungibile` (with automatic retry).

## Reading traffic

- Each captured record appears as a row: sequence number, ESP32 millisecond timestamp, bus and direction (`RX`/`TX`), frame description, HEX bytes, and an ASCII preview with non-printable bytes shown as `·`.
- Serial records are blocks of bytes as read by the UART, not decoded application messages. CAN rows show the identifier in HEX, `STD`/`EXT`, optional `RTR`, and DLC.
- The cards at the top show per-bus record counters and the current speed (`attiva` / `non disponibile`).
- The footer line shows device uptime, history overwrites, and CAN loss counters.
- The table follows new records automatically. Scrolling up suspends autoscroll; the `Scorrimento automatico` checkbox re-enables it and jumps to the latest record.

## View controls

| Action | Result |
| --- | --- |
| `Interfaccia` selector | Filters the table to one bus. |
| `Pausa vista` | Freezes rendering; capture on the device continues and the safety state (TX armed or not) keeps refreshing. |
| `Pulisci vista` | Clears the browser-side view; the device history is not erased. |
| `Esporta CSV` | Downloads the currently retained records matching the filter as a `;`-separated CSV (UTF-8 with BOM). While paused, these may differ from the frozen table. Cells that look like spreadsheet formulas are prefixed with `'` to neutralize them. |

The browser keeps at most 500 records; the device history holds 128 records in RAM and overwrites the oldest. If the browser falls behind, the gap is reported as missed records.

## Speed control

`Controlla velocità` starts a passive scan: the firmware samples RX edges on both serial lines for about 3 seconds and steps CAN through 500/250/125/1000 kbit/s (about 1.2 s per candidate, stopping after three received frames). During the scan TX controls are disabled.

- Serial results are shown as estimates (`RS232 <n> baud · RS485 <n> baud`). `non rilevata` means no conclusive traffic; a silent line cannot be identified. Estimates are never applied automatically: confirm the value against the device documentation, then apply it with the speed selector on the card and `Applica`.
- A detected CAN rate is applied directly; if nothing is found the previous speed is restored.
- Applied speeds last until the next reboot; defaults are compile-time settings.

## Sending a command

1. Press `Abilita trasmissione` and confirm the dialog. The tag changes to `RX + TX`; CAN switches to normal mode and sends ACKs on the bus.
2. Fill the form: interface, `HEX` or `Testo UTF-8`, payload. For CAN also set the identifier (HEX without `0x`), standard/extended format, data or remote (RTR) frame, and DLC for RTR.
3. Press `Invia una volta`. The firmware validates and queues the request (202), not a delivery confirmation. Read the separate device-status message; a successful driver write/queue adds a `TX` row. Reboot or driver failure can prevent execution. Never resend automatically.
4. TX disarms automatically 60 seconds after the last send, or immediately with `Disabilita trasmissione`.

A `TX` row means the firmware wrote the bytes or queued the CAN frame. It is not a delivery confirmation from the device.

The request result stays separate from the device's shared status, so polling
does not erase an HTTP rejection or timeout. Other clients can change device
status; it is not a per-request execution receipt.

Optionally pick a `Sistema` and `Comando salvato` and press `Carica nei campi`: the form and speed fields are filled from `web/systems.json`. Loading a profile never arms TX and never sends anything. The bundled profile is an example, not a validated device protocol.

## API access

The dashboard is a client of a small HTTP API. Direct use (for example from a script) is documented in [api.md](api.md). Mutating requests require the `X-TraceLink` boot token shown to the page after each boot; cross-origin forms cannot arm or send.

## Disconnects and errors

- If the board reboots, the dashboard detects the new boot token, clears the view, and resumes from the new history.
- `Ricarica la pagina` (HTTP 403) means the boot token is stale: reload the page.
- `Operazione non consentita` (HTTP 409) means the action conflicts with the current state (for example scanning while TX is armed).
- HTTP 400 responses describe the rejected field (invalid HEX, unsupported rate, CAN identifier out of range, RTR with data, and so on).
