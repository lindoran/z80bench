# z80bench

`z80bench` is a GTK4 desktop workbench for Z80 ROM annotation and export.
It loads a ROM project directory, disassembles CODE regions with `z80dasm`,
tracks labels/comments/segments/symbols, and exports z88dk-compatible source.

## Current Shape (April 2026)

- C99 backend with a GTK4 UI
- Project files live in one directory: `rom.bin`, `listing.mnm`,
  `annotations.ann`, `segments.map`, `symbols.sym`
- Segments, symbols, and annotations are editable in-app
- Segment types include direct data ranges: `DIRECT_BYTE`, `DIRECT_WORD`,
  and `DEFINE_MSG` (rendering as `DEFB` / `DEFW` / `DEFM`)
- Operand literals are normalized to `0x` form and replaceable with
  resolved symbol/label names where available
- Session persistence is enabled for:
  - recent projects
  - last project path
  - auto-open-last preference
- Bottom dock includes hex-analysis filters (text runs, high-bit runs,
  and smart address-in/out-of-ROM highlighting)
- Side panels use stateless click behavior:
  - single click jumps in listing
  - double click opens editor dialog
- Segment + symbol dialogs now share a similar flow:
  - `Add`/`Update`/`Remove` apply immediately
  - `Close` exits dialog
- Notes fields have been removed from segment/symbol dialogs
- The build succeeds with `make`

## Known Rough Edges

- The listing view is still a `GtkListBox`-based implementation, not the
  virtualized `GtkListView` described in the older design notes
- The references workflow is still incomplete
- Segment editor `Close` commits/reloads+jumps; window `X` close path still
  follows normal destroy behavior and should be treated as a separate flow
- Legacy `project.*` filenames still open, but new projects save with the
  clearer file names above

## Docs

- [`docs/STATUS.md`](docs/STATUS.md) - current state and known gaps
- [`docs/DESIGN.md`](docs/DESIGN.md) - project model and file formats
- [`docs/DISASM.md`](docs/DISASM.md) - disassembler interface
- [`docs/UI.md`](docs/UI.md) - UI layout and widget plan
- [`docs/SEGMENTS_RULES.md`](docs/SEGMENTS_RULES.md) - rules baseline for parent/subrange behavior

## Build

```bash
make
```
