# z80bench

`z80bench` is a GTK4 desktop workbench for Z80 ROM annotation and export.
It loads a ROM project directory, disassembles CODE regions with `z80dasm`,
tracks labels/comments/segments/symbols, and exports z88dk-compatible source.

## Current Shape

- C99 backend with a GTK4 UI
- Project files live in one directory: `rom.bin`, `listing.mnm`,
  `annotations.ann`, `segments.map`, `symbols.sym`
- Segments, symbols, and annotations are editable in-app
- The build currently succeeds with `make`

## Known Rough Edges

- The docs are being refreshed to match the current code
- The listing view is still a `GtkListBox`-based implementation, not the
  virtualized `GtkListView` described in the older design notes
- The references workflow is still incomplete
- Segment nesting and overwrite behavior is being tightened up
- Legacy `project.*` filenames still open, but new projects save with the
  clearer file names above

## Docs

- [`docs/STATUS.md`](docs/STATUS.md) - current state and known gaps
- [`docs/DESIGN.md`](docs/DESIGN.md) - project model and file formats
- [`docs/DISASM.md`](docs/DISASM.md) - disassembler interface
- [`docs/UI.md`](docs/UI.md) - UI layout and widget plan

## Build

```bash
make
```
