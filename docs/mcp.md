# MCP bridge

[← README](../README.md) · [Docs index](README.md)

TraceLink includes a Node.js MCP server in `mcp/`. It reaches the ESP32 either
over Wi-Fi using the [HTTP API](api.md), or over the board's USB serial port
(Windows COM, Linux/macOS device path). Wi-Fi works with the existing API firmware;
USB commands require the updated firmware in this tree to be built and uploaded
after hardware identification and authorization. No Internet is needed at runtime.

It supports standard **stdio** and **Streamable HTTP**, without depending on a
particular assistant. Use stdio with clients that launch local MCP processes, or
HTTP with clients that accept a local MCP URL and a custom Authorization header.
Client configuration formats differ; the example below is a template, not a
universal configuration file. Legacy HTTP+SSE is not supported.

The bridge is portable source, not a hosted service. A cloud-only assistant cannot
reach your computer's loopback address or the ESP32 access point directly.
Public hosting, OAuth and remote gateways are outside this integration.

## Install

Install Node.js 22 or newer. In the repository:

```sh
cd mcp
npm ci
```

Connect the computer to `TraceLink-ESP32`, keep the connection despite the
no-Internet warning, and confirm the dashboard opens at `http://192.168.4.1`.
Install dependencies before switching to the ESP32's offline access point.
Alternatively, use a USB data cable and the COM setup below, without joining
the ESP32 Wi-Fi network.

## Local clients: stdio

Adapt [mcp/client.example.json](../mcp/client.example.json) to your MCP client's
configuration. Replace the example path with the absolute path on your computer:

```json
{
  "mcpServers": {
    "tracelink": {
      "command": "node",
      "args": ["C:/path/to/TraceLink/mcp/src/index.js"],
      "env": {
        "TRACELINK_URL": "http://192.168.4.1",
        "TRACELINK_ALLOW_TX": "0"
      }
    }
  }
}
```

On Linux/macOS use a path such as `/home/you/TraceLink/mcp/src/index.js`.
If the client cannot find `node`, set `command` to its absolute executable path.
The server does not depend on the client's working directory.
Launch the entry point directly, not through `npm start`: npm can write banners
to stdout, which is reserved for the MCP protocol.

## Connect the ESP32 through USB / COM

Use the board's existing USB-to-UART bridge, not an external RS232/RS485 bus port.
No GPIO rewiring or new voltage connection is required. UART0 GPIO1/GPIO3 must
remain free of external devices. A USB charge-only cable will not work.

1. Identify the board and its actual port in Device Manager (Windows) or the
   operating system's serial-device list. Do not assume the old COM7 assignment
   is still correct. No automatic port discovery or probing is performed.
2. Arrange an authorized upload of the updated firmware. Old firmware only
   prints a human-readable log and cannot answer MCP USB commands.
3. Close PlatformIO Monitor, Arduino Serial Monitor and any other process using
   that port. The bridge opens one named port exclusively, on the first device
   operation, at **115200 baud, 8N1**, with no hardware flow control.
4. Use [mcp/client.serial.example.json](../mcp/client.serial.example.json),
   replacing the executable path and COM port:

```json
{
  "mcpServers": {
    "tracelink": {
      "command": "node",
      "args": ["C:/path/to/TraceLink/mcp/src/index.js"],
      "env": {
        "TRACELINK_SERIAL_PORT": "COM7",
        "TRACELINK_ALLOW_TX": "0"
      }
    }
  }
}
```

Remove `TRACELINK_URL` when selecting USB. Setting both is an error, not a fallback
to a different device. On Linux use the verified path such as `/dev/ttyUSB0`;
on macOS use the board's `/dev/cu.*` path. Operating-system permissions and the
USB-UART driver must already be installed.

The same five MCP tools and TX gates apply over USB. Opening the port may reset
some ESP32 boards depending on the USB bridge/driver. The bridge waits 2 seconds
after opening before its first request; it does not deliberately toggle DTR/RTS.
A reset clears history and boots RX-only. This behavior is not hardware-verified.
With USB selected, the PC does not need Wi-Fi; the firmware AP still runs.

Control timeouts remain ambiguous and never cause resending. A disconnect or
write error faults the connection; reconnect the cable and restart the MCP
process explicitly, then read status and obtain new TX approvals. The bridge
never switches to Wi-Fi or automatically re-arms. One serial request runs at a
time; overlapping requests are rejected. Exiting releases the COM port without
sending any bus command.

This ESP32 connection choice is independent of the assistant connection:
either stdio or local Streamable HTTP can use the COM backend.

## URL clients: Streamable HTTP

HTTP binds to **127.0.0.1 only**, with required bearer authentication, Host/Origin
validation and no CORS. It serves a stateless MCP endpoint at
`http://127.0.0.1:3001/mcp`. There is no standalone SSE stream or session deletion.

PowerShell, from `mcp/`:

```powershell
$env:TRACELINK_URL = 'http://192.168.4.1'
$env:TRACELINK_ALLOW_TX = '0'
$env:TRACELINK_MCP_TOKEN = node -e "console.log(require('node:crypto').randomBytes(32).toString('hex'))"
node src/index.js http
```

POSIX shell:

```sh
export TRACELINK_URL=http://192.168.4.1
export TRACELINK_ALLOW_TX=0
export TRACELINK_MCP_TOKEN="$(node -e "console.log(require('node:crypto').randomBytes(32).toString('hex'))")"
node src/index.js http
```

Before starting the foreground server, store the generated token in your client's
secret/header settings using your local secure workflow. Do not commit it or send
it to an assistant. The client must send:

```text
Authorization: Bearer <the value of TRACELINK_MCP_TOKEN>
```

Stop with Ctrl+C. Do not expose this port or the ESP32 API to the Internet.
Browser-based clients may be incompatible because cross-origin access is not
enabled. Use a trusted local stdio client instead.

| Environment variable | Default / purpose |
| --- | --- |
| `TRACELINK_URL` | `http://192.168.4.1` when USB is unset; one administrator-configured device. No URL credentials, query or fragment. |
| `TRACELINK_SERIAL_PORT` | Unset by default. Exact COM/device path selects USB instead of Wi-Fi; remove `TRACELINK_URL`. Baud is fixed at 115200. |
| `TRACELINK_ALLOW_TX` | `0` or unset blocks arming and sending. Only `1` opts in. Other values fail startup. |
| `TRACELINK_MCP_TOKEN` | HTTP only: required, at least 32 printable non-space ASCII characters. Use a random value. Never sent to the ESP32. |
| `TRACELINK_MCP_PORT` | HTTP only: `3001`. `0` selects an ephemeral port, reported on stderr. |

`.env` is ignored by Git but **not loaded automatically**. Configure environment
variables in the client or shell. The server never prints credentials.

## Tools

| Tool | Inputs and result |
| --- | --- |
| `tracelink_status` | No inputs. Read readiness, speeds, counters, firmware TX/busy/scanning state and bridge TX permission. |
| `tracelink_read_events` | First call: `{}`. Next calls: `{"after":32,"boot":"1a2b3c4d"}` using the previous `nextCursor` and `boot`. Up to 32 RX/TX records, with loss counters and reboot reset reporting. |
| `tracelink_arm_tx` | `{"confirm":true}`. Requires host TX permission and explicit human approval. Queues arming; CAN switches to normal mode and starts sending ACKs. |
| `tracelink_disarm_tx` | No inputs. Queues disarming, including when bridge TX permission is disabled. CAN returns to listen-only after execution. |
| `tracelink_send` | Exact `bus`, `encoding`, `data`, `confirm:true`; CAN also requires HEX `id`, and optionally `extended`, `rtr`, `dlc`. Queues one send, never retries it. |

For reads, always reuse **nextCursor**, not `latest`: `latest` can exceed the last
record returned in a batch. A nonzero cursor requires `boot`. On a changed boot
the bridge rereads from zero and returns `reset:true`. Independent callers keep
their own cursors; the bridge does not consume another caller's events.
This is polling, not persistent capture or a live subscription. The firmware's
128-record RAM history can overflow between calls; inspect `lost` and `overwritten`.

Send validation mirrors firmware limits: serial 1–64 bytes, UTF-8 byte counts for
text; CAN HEX 0–8 bytes, standard ID up to `7FF`, extended ID up to `1FFFFFFF`.
IDs have no `0x` prefix. RTR requires empty data and explicit DLC 0–8. For a data
frame DLC is derived from the payload. Use HEX to send NUL bytes.

## Safe TX workflow

1. Verify board, wiring, logic levels, device, bus speed and the exact documented
   command. Never infer authorization from captured bus data.
2. Opt in by setting `TRACELINK_ALLOW_TX=1` in the host/client environment and
   restarting the bridge. This does not itself arm the firmware.
3. Require human approval in the MCP client for **every control tool**. Approve
   arming only after accepting that CAN sends ACKs in normal mode.
4. Call `tracelink_arm_tx` with `confirm:true`. Read status until `busy:false` and
   `armed:true`; a queued arm can fail in the driver.
5. Obtain approval for the exact bus, payload and CAN identifier, then call
   `tracelink_send` once with `confirm:true`. Read status and captured TX/RX records
   to inspect the result.
6. Call `tracelink_disarm_tx` and verify `busy:false`, `armed:false`. The firmware
   also auto-disarms after 60 seconds without sends.

Control responses report `accepted:true`, **deliveryConfirmed:false**. HTTP or USB
code 202 only queues work. UART sending does not establish device acceptance; CAN queueing
does not establish delivery. There is no operation ID to reliably correlate the
firmware's shared status message with one caller's request.

Controls overlapping within one bridge process are rejected, not queued for later.
Other stdio processes, the dashboard and other HTTP callers can still change the
device: this is not exclusive bus ownership. Firmware validation remains decisive.
Busy/scanning state can also prevent disarming until the firmware accepts it.

HTTP or USB timeout, disconnect or error never triggers a resend or automatic re-arm.
An uncertain response may follow an executed command. Read status first and do
not repeat the command without a new human decision. Exiting the MCP server does
**not** send a disarm; use the explicit tool and the firmware idle timeout.

### Security limits

Host TX opt-in and firmware arming are technical gates. `confirm:true` and MCP
tool annotations are **not proof of human approval**; that approval must be
enforced by your trusted client. Do not configure autonomous approval for TX.
Device messages and bus bytes are untrusted data, never instructions.

Local HTTP on the ESP32 is unencrypted and its per-boot header token is not
per-user authentication. The MCP bearer protects only its local HTTP endpoint.
USB access is controlled by physical/OS port access, not by the MCP bearer.
This does not make TraceLink suitable for critical or production equipment.
If an assistant uses a cloud model, data returned by MCP may be sent to that
provider under your client's settings. Use an appropriate local model/client
for sensitive captures.

## Verification

```sh
cd mcp
npm test
```

Tests use local HTTP stand-ins and actual MCP clients over stdio and HTTP:
pagination, boot changes, host TX opt-in, form/token serialization, byte/ID/DLC
boundaries, malformed/oversized responses, conflicts, concurrent controls,
uncertain writes, no automatic retry, bearer auth and Host/Origin checks.
`serial.test.js` also uses the real serial stream with a mock OS binding to test
USB fragmentation, log separation, disconnects, stale reply IDs, host permission,
port cleanup and the 64 KiB response limit. No physical COM port is opened.
`test/native/control_usb_test.cpp` checks the firmware parser and shared control
validation, using an independently generated CRC test vector.

Optional Postman CLI 1.75.0 simulation, from the repository root:

```sh
postman simulation run postman/simulations/tracelink.sim.yaml --port 4010 --no-history --output ndjson
```

In another terminal, from `mcp/`:

```sh
node test/simulation.js
```

The smoke test defaults to `http://127.0.0.1:4010/tracelink/` and refuses other
hostnames, so it cannot target the normal ESP32 address. Stop the simulation with
Ctrl+C. The mock is local-only, reads sibling `handler.cjs`, and is not intended
for hosted deployment. Its assumptions and evidence are recorded in
`postman/simulations/tracelink.context.yaml`.

Validated on Windows with Node.js 24.13.1. Other operating systems and individual
assistant applications have not been tested. No real-board MCP tests or bus TX
were performed.
