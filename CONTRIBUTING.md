# Contributing to TraceLink

## Documentation conventions

These rules are shared by every project under [camiu01](https://github.com/camiu01) and keep the
documentation consistent from one repository to the next. Follow them when you add or edit any
Markdown file, and update this section in every repository if a rule changes.

### README.md

- Starts with a centered header: title, one-line tagline (`###`), badges on one line, a hero image
  (photo, GIF, render or screenshot), then quick links separated by ` · `.
- The intro prose follows the header, before the first section.
- Top-level sections, in this order, omitting the ones that do not apply:
  **What it does**, **Getting started**, **Usage**, **Configuration**, **How it works**,
  **Limitations**, **Development**, **License**.
- When a section is renamed, keep the old anchor with `<a id="old-anchor"></a>` above the heading.

### TODO.md

- Title `# TODO`, followed by `> Last reviewed: YYYY-MM-DD`.
- One `##` section per **area** of the project (for example Firmware, Web dashboard, Docs, CI).
  Areas are ordered by how much open work they hold.
- Inside an area, open items come first as `- [ ] **Short title** — context`, then completed
  items as `- [x] ...`. Use `Blocked:` or `Needs hardware:` at the start of the context when an
  item cannot progress.
- Completed items that shipped in a release move to `CHANGELOG.md`; keep only recent ones here.

### CHANGELOG.md

- Follows [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/).
- Newest first: `## [Unreleased]`, then `## [x.y.z] - YYYY-MM-DD` (or `## YYYY-MM-DD` for projects
  without version numbers).
- Entries are grouped under `### Added`, `### Changed`, `### Deprecated`, `### Removed`,
  `### Fixed` and `### Security`, omitting empty groups.

### docs/

- `docs/README.md` is the index: the title, then a table of every document with a one-line
  description, then any reading paths.
- Every other file in `docs/` starts with its `# Title` followed by the navigation line
  `[← README](../README.md) · [Docs index](README.md)`.
- File-level comments required by a project (such as `@file`/`@brief`) stay above the title.

### Commits and files

- No AI attribution lines (such as `Co-Authored-By`) in commit messages or pull requests.
- Preserve each file's existing line endings and encoding.
