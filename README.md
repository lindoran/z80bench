# The Z80 Workbench

## z80bench (Version 1)

`z80bench` is a GTK4 desktop workbench for annotating Z80 ROMs and exporting z88dk-compatible assembly output.

This README documents the Version 1 workflow and interfaces.

## Why?
I (mostly Codex) made a tool to make it less cumbersome to translate listings and document ROM code. This fits somewhere between a bunch of CLI tools, text-based DOS disassembly tools of old, and Ghidra. I like to make my own tools and this was exactly what I wanted.

## What it's not
Not a forensic tool that can be used to decompile C code. This isn't a guaranteed silver bullet for breaking down a ROM to find its structure; this is a documentation tool.

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

## CLI Quick Check

```bash
./z80bench-cli help
./z80bench-cli project info <project_dir>
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

## Chatbot CLI Workflow

Yes, a chatbot/agent can safely drive project updates through `z80bench-cli`.

Recommended loop:

1. Read context (`project info`, `line get/list`, `annotation get`, `segment list`, `symbol list`).
2. Plan edits in small batches.
3. Apply edits (`annotation set`, `segment add/update/remove`, `symbol add/update/remove`).
4. Verify results by reading back the changed addresses/entries.
5. Save/export when needed (`project save`, `project export`).

Example batch flow:

```bash
cat <<'EOF' | ./z80bench-cli batch <project_dir>
annotation set 0x0050 label INIT
annotation set 0x0050 comment Entry point from RESET
symbol add INIT 0x0050 JUMP_LABEL
segment add 0x008B 0x008C DIRECT_BYTE inline_bytes
EOF
```

Notes:

- Addresses can be decimal or `0x` hex.
- `batch` lines omit the project directory (it is supplied once on the command line).
- `annotation set` currently supports fields: `label`, `comment`, `block`.

## Historical Design/Iteration Docs

Older AI iteration notes and design/status docs are kept for reference in:

- `docs/ai-iteration-history/`

These are useful for project history, but not required for normal usage.

## License

This project is licensed under the MIT License.
See `LICENSE` for the full text.
GitHub: https://github.com/lindoran/z80bench
