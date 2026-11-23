# TraceLink documentation

[← README](../README.md)

## Documents

| Document | What it covers |
| :--- | :--- |
| [HTTP API reference](api.md) | HTTP endpoints, fields, and error behavior |
| [Firmware architecture](architecture.md) | Tasks, state and buffer ownership, scan/TX state machines |
| [Audit: 17 November 2026](audit-2026-11-17.md) | Fixed bugs, remaining source-level risks, evidence and approval boundaries |
| [Hardware and wiring](hardware.md) | Boards, modules, pin map, electrical boundaries, validation status |
| [MCP bridge](mcp.md) | Portable MCP bridge, client setup, protected TX and local tests |
| [Testing and validation](testing.md) | Test commands, what is covered, what is not |
| [Troubleshooting](troubleshooting.md) | Observed issues and diagnostics |
| [Usage guide](usage.md) | Main workflow, dashboard controls, API examples |

## Reading paths

- **Users:** start with the [README](../README.md), then [usage](usage.md) for the dashboard workflow and [troubleshooting](troubleshooting.md) when something misbehaves.
- **Developers:** [architecture](architecture.md), [HTTP API](api.md), [testing](testing.md), plus the conventions in [agent.md](../agent.md).
- **Technical readers:** [architecture](architecture.md) and [api.md](api.md) describe the firmware as implemented; [hardware](hardware.md) covers the electrical side.
- **Maintainers:** [PROJECT_STATUS.md](../PROJECT_STATUS.md) for verification status and resume points, [TODO.md](../TODO.md) for open work, [CHANGELOG.md](../CHANGELOG.md) for history.
