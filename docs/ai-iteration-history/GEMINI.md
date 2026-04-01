# z80bench — Quick Contributor Context

This file is a short orientation note. Source of truth is:

- `README.md` (current shape)
- `docs/STATUS.md` (what works / rough edges)
- `docs/DESIGN.md` (project model + file formats)
- `docs/UI.md` (current implemented UI notes + broader spec)

## What z80bench is

A GTK4 desktop workbench for annotating Z80 ROMs and exporting assembler output.
Core data is a project folder with:

- `rom.bin`
- `listing.mnm` (derived)
- `annotations.ann`
- `segments.map`
- `symbols.sym`

Legacy `project.*` names still load.

## Current architecture

- Backend modules in C (`project`, `disasm`, `annotate`, `memmap`, `symbols`, `export`)
- GTK4 UI (`main`, `ui_listing`, `ui_panels`)
- CLI utility (`z80bench-cli`) for inspect/validate/migrate flows

## Important behavior assumptions

- `rom.bin` is read-only after import.
- `listing.mnm` is derived/regenerable.
- Listing bytes come from ROM data, not parsed from z80dasm output.
- Labels/comments/blocks/xref come from annotation merge and UI edits.
- Segment and symbol dialogs apply add/update/remove immediately in memory.
- Side panels use stateless interaction:
  - single click jumps listing
  - double click opens dialog

## Keep in mind while changing code

- Prefer backend logic in core modules, keep UI modules thin.
- Preserve on-disk compatibility for existing project files.
- Validate with:
  - `make`
  - `./z80bench-cli validate <project_dir>`
- If UI behavior differs from docs, update docs in the same change.
