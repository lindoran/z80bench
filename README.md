# z80bench

`z80bench` is a GTK4 desktop workbench for annotating Z80 ROMs and exporting z88dk-compatible assembly output.

## What It Does

- Opens a ROM project directory
- Disassembles CODE ranges using `z80dasm`
- Lets you edit labels, comments, segments, and symbols
- Persists project/session data to flat text files
- Exports annotated output as `.lst` and `.asm`

## Project Layout

A project is a folder with these files:

- `rom.bin` (ROM binary, read-only after import)
- `listing.mnm` (derived disassembly cache)
- `annotations.ann` (labels/comments/regions/meta)
- `segments.map` (named address ranges)
- `symbols.sym` (named addresses/constants)

Legacy `project.*` names still load for compatibility, but current saves use the names above.

## Build

```bash
make
```

## Run

```bash
./z80bench
```

## CLI Validation

```bash
./z80bench-cli validate <project_dir>
```

## UI Notes

Current UI behavior includes:

- Listing + side panels for segments, symbols, and annotations
- Side panel interaction:
  - single click jumps in listing
  - double click opens edit dialog
- Segment and symbol dialogs apply `Add`/`Update`/`Remove` immediately
- Bottom hex-analysis dock with text/high-bit/address filters

## Data And Workflow Notes

- `rom.bin` is treated as immutable source input
- `listing.mnm` is regenerable cache data
- Operand literals normalize to `0x` style
- Symbol/label resolution is applied where available

## Historical Design/Iteration Docs

Older AI iteration notes and design/status docs are kept for reference in:

- `docs/ai-iteration-history/`

These are useful for project history, but not required for normal usage.

## License

This project is licensed under the MIT License.
See `LICENSE` for the full text.
