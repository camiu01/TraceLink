# TraceLink documentation

Reading paths:

- **Users:** start with the [README](../README.md), then [usage](usage.md) for the dashboard workflow and [troubleshooting](troubleshooting.md) when something misbehaves.
- **Developers:** [architecture](architecture.md), [HTTP API](api.md), [testing](testing.md), plus the conventions in [agent.md](../agent.md).
- **Technical readers:** [architecture](architecture.md) and [api.md](api.md) describe the firmware as implemented; [hardware](hardware.md) covers the electrical side.
- **Maintainers:** [PROJECT_STATUS.md](../PROJECT_STATUS.md) for verification status and resume points, [TODO.md](../TODO.md) for open work, [CHANGELOG.md](../CHANGELOG.md) for history.

| Document | Contents |
| --- | --- |
| [usage.md](usage.md) | Main workflow, dashboard controls, API examples |
| [architecture.md](architecture.md) | Tasks, state and buffer ownership, scan/TX state machines |
| [api.md](api.md) | HTTP endpoints, fields, and error behavior |
| [mcp.md](mcp.md) | Portable MCP bridge, client setup, protected TX and local tests |
| [hardware.md](hardware.md) | Boards, modules, pin map, electrical boundaries, validation status |
| [testing.md](testing.md) | Test commands, what is covered, what is not |
| [audit-2026-11-17.md](audit-2026-11-17.md) | Fixed bugs, remaining source-level risks, evidence and approval boundaries |
| [troubleshooting.md](troubleshooting.md) | Observed issues and diagnostics |
