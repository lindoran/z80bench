# z80bench — Gemini CLI Project Context

## What this is

A Z80 ROM annotation workbench. C library backend with a GTK4 desktop UI.
The goal is a focused tool for reverse engineering Z80 ROM images — currently
centred on the VZ200 ROM, a derivative of Microsoft Level II BASIC.

Read the spec documents before implementing anything:

- `docs/DESIGN.md` — architecture, all five project file formats, module
  responsibilities, export formats, design constraints
- `docs/DISASM.md` — disassembler interface: `DisasmLine` struct, function
  signatures, z80dasm subprocess adapter, orphan byte detection
- `docs/UI.md` — GTK4 window layout, listing view, right panel column,
  colour scheme, widget map, implementation notes

---

## Technology

- Language: C, C99
- UI toolkit: GTK4 (system packages, not vendored)
- Disassembler: z80dasm (subprocess, must be in PATH)
- Assembler output syntax: z88dk / z80asm
- Build: Make
- No other runtime dependencies

---

## Module Responsibilities

Each source file has a single clear responsibility. Do not mix concerns between
modules. The full list is in `docs/DESIGN.md` under Source Layout.

| File | Owns |
|------|------|
| `main.c` | GTK4 app entry, window creation |
| `ui_listing.c` | Listing view widget and inline search |
| `ui_panels.c` | Segments, symbols, references, annotation panels |
| `ui_search.c` | Placeholder / legacy search module |
| `project.c` | Project open/create/save, coordinates all five files |
| `disasm.c` | z80dasm subprocess adapter, `.mnm` writer/reader |
| `annotate.c` | `.ann` parser/writer, `xref_build()` |
| `memmap.c` | `.map` parser/writer |
| `symbols.c` | `.sym` parser/writer, colour tag resolution |
| `export.c` | `.lst` and `.asm` export |
| `z80bench.h` | Public library API header |

---

## Key Data Structures

`DisasmLine` is the fundamental unit — defined in full in `docs/DISASM.md`.
`XrefEntry` is the cross-reference entry — defined in `docs/DESIGN.md`.
`RegionType` enum includes `RTYPE_ORPHAN` for bytes z80dasm could not decode.
`XrefType` enum: `XREF_ADDR` and `XREF_ORPHAN`.

---

## Strict Rules

- **Never modify `project.bin` after import.** It is read-only.
- **`project.mnm` is always derived.** Never treat it as a source of truth.
  Always regenerable from `project.bin` + regions in `project.ann`.
- **Bytes come from `project.bin`.** The `DisasmLine.bytes[]` array is populated
  by reading the ROM blob directly using `offset` + `byte_count`. Do not parse
  byte data from z80dasm output.
- **z80dasm supplies mnemonic and operand text only.** Do not reimplement any
  decode logic that z80dasm already handles.
- **Annotation fields are left empty by `disasm.c`.** `label`, `comment`,
  `block`, `xref` are populated by `annotate_merge()` after `disasm_range()`
  returns. `sym_name` and `sym_type` are populated by `symbols_resolve()`.
- **`disasm_range()` is the only place z80dasm is invoked.** No other module
  shells out to z80dasm directly.
- **Regen runs off the main thread.** z80dasm subprocess must not block the
  GTK4 event loop.
- **The listing is still being modernised.** The current code uses a
  `GtkListBox`-based implementation; keep the virtual list target in mind, but
  do not assume it is already in place.

---

## Code Style

- British spelling in all comments, UI strings, and log messages:
  `colour` not `color`, `initialise` not `initialize`, `recognised` not `recognized`
- Function names: `module_verb_noun` e.g. `disasm_write_mnm`, `annotate_merge`,
  `xref_build`
- Every public function has a block comment matching the style in `docs/DISASM.md`
- Error returns: functions return 0 on success, -1 on error with `errno` set,
  unless otherwise specified in the spec
- No global mutable state outside of the single project context struct

---

## File Formats

All five project file formats are fully specified in `docs/DESIGN.md`.
Do not invent new fields or sections without checking the spec first.

`.sym` uses `;` for comments (z88dk compatibility).
All other text files use `#` for comments.
All addresses use `0x` prefix.
Byte offsets in `.mnm` and `.ann` are decimal integers (ROM blob index, not load address).

---

## What Not to Do

- Do not add cycle count fields — removed from scope, see Future section of DESIGN.md
- Do not add flow classification enums — removed from scope
- Do not create an HTTP server or JSON API — the architecture is a direct C library
- Do not create files not listed in the Source Layout without asking first
- Do not use `GtkTreeView` for the listing view
- Do not read byte data from z80dasm stdout
