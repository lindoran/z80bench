# z80bench — UI Specification

> Implementation note (March 2026):
> This document contains both target/spec language and historical notes.
> For current behavior, prefer this section plus `docs/STATUS.md`.

## Current UI Behavior (Implemented)

- Main window: toolbar + listing (left) + right panel stack (segments, symbols, annotation)
  with a bottom dock (hex dump + search).
- Toolbar includes:
  - `Open Project`, `Open Recent`, `New Project`, `Save`
  - `Auto-open last` toggle
  - `Export .LST`, `Export .ASM`
- Side panels (`SEGMENTS` / `SYMBOLS`) do not maintain persistent row selection state.
  - Single click: jump listing.
  - Double click: open editor dialog.
- Segment dialog:
  - `Add` / `Update` / `Remove` apply immediately in memory.
  - `Close` closes dialog and triggers segment save/reload+jump flow when needed.
  - Notes input is removed.
- Symbol dialog:
  - `Add` / `Update` / `Remove` apply immediately in memory.
  - `Close` closes dialog and jumps to last symbol address touched.
  - Notes input is removed.
- Annotation `Label` edits sync to symbol table (`SYM_JUMP_LABEL`).
- Symbol dialog edits for `SYM_JUMP_LABEL` sync back to listing/annotation labels.
- Listing remains `GtkListBox` based (not virtualized `GtkListView` yet).
- Listing search is `Find Next` driven (not live filtering). In mnemonic mode,
  search matches mnemonic + symbol + label + operand text.
- Segment list rows use compact direct-data badges:
  - `DEFB` (direct byte), `DEFW` (direct word), `DEFM` (define message)
- Hex dock includes configurable filters:
  - `Text runs >=3`
  - `High-bit >=3`
  - `Addr in ROM`
  - `Addr out ROM`
  - `Skip text overlap`
- References panel is still planned, not shipped.

Desktop workbench application. GTK4, dark mode by default (follows system theme,
requests dark colour scheme on startup). Single main window, no MDI, one project
open at a time.

---

## Window Layout

Golden ratio split: listing panel takes **61.8%** of horizontal space, right
panel column takes the remaining **38.2%**. Minimum window size 1024×600.

```
┌─────────────────────────────────────────────────────────────────────┐
│  Title bar: z80bench — <project_name>    z80 · z80dasm · z88dk      │
├──────────────────────────────────────────────────────────────────────┤
│  Toolbar                                                             │
├────────────────────────────────────────┬─────────────────────────────┤
│                                        │  Segments panel             │
│                                        ├─────────────────────────────┤
│  Listing panel  (61.8%)                │  Symbols panel              │
│                                        ├─────────────────────────────┤
│                                        │  References panel           │
│                                        ├─────────────────────────────┤
│                                        │  Annotation panel           │
├────────────────────────────────────────┴─────────────────────────────┤
│  Status bar                                                          │
└──────────────────────────────────────────────────────────────────────┘
```

---

## Title Bar

```
z80bench — <project_name>          z80 · z80dasm · z88dk
```

Modified indicator: append ` *` to project name when unsaved changes exist.

---

## Toolbar

Single row. Left-aligned controls, separator, search, flex spacer, right-aligned
export buttons.

```
[Open] [Save] [Regen]  |  [___Search listing___]     [Add Segment] [Add Symbol]  |  [Export .LST] [Export .ASM]
```

- **Open** — folder chooser dialog, selects a project folder
- **Save** — write all modified text files to disk; no-op if clean
- **Regen** — re-invoke z80dasm on all CODE regions, rewrite `listing.mnm`,
  reload listing. Shows confirmation dialog if unsaved annotation edits exist.
- **Search** — monospace input plus `Find Next` workflow.
  Search runs against the active mode (Addr/Mnem/Comment). In mnemonic mode,
  matches include mnemonic, symbol, label, and operand text.
- **Add Segment** — opens the Segment Editor dialog
- **Add Symbol** — opens the Symbol Editor dialog
- **Export .LST** — file save dialog, writes annotated listing
- **Export .ASM** — file save dialog, writes z88dk assembler source

---

## Listing Panel

Fills the left 61.8% of the main area. Contains a fixed column header row and
a scrollable virtualised list below it.

### Column Header Row

Fixed — does not scroll. Font: monospace 10px, muted colour.

```
ADDR    BYTES              MNEM    OPERANDS              COMMENT
```

| Column | Width | Notes |
|--------|-------|-------|
| ADDR | 52px | 4-digit hex address, uppercase |
| BYTES | 88px | Space-separated hex bytes from `rom.bin`, uppercase |
| MNEM | 50px | Mnemonic from z80dasm or data formatter |
| OPERANDS | flex | Fills remaining space |
| COMMENT | auto | Inline comment, right side, muted, truncated with ellipsis |

The BYTES column data comes from `DisasmLine.bytes[]` which is populated by
reading `rom.bin` directly. z80dasm is not the source of this data.

### Row Types

**Block comment row** — rendered before the label row when a `[block]` annotation
exists at an address. Full-width, comment colour, italic, prefixed with `; `.
Multi-line blocks render as multiple full-width rows. Not selectable.

**Label row** — rendered before the instruction row when a `[label]` annotation
exists at that address. Shows the label name followed by `:` in label colour.
Not independently selectable — clicking it selects the instruction row below it.

**Instruction row** — the main selectable row type. All columns as above.
Clicking selects it; the annotation panel updates to reflect the selected address.

**Data row** — same layout as instruction row. Mnemonic is `DEFB`, `DEFW`, or
`DEFM` as generated by `disasm.c`. BYTES column shows the raw data bytes.

**Gap row** — same as data row. Mnemonic is `DEFB`. Distinguished from data rows
by a small `[GAP]` indicator in muted colour at the far right.

**Section banner row** — full-width divider at the start of each segment
region. Shows region name, address range, and type. Not selectable.
```
; ══════ ROM  0x0000 – 0x3FFF  [ROM] ══════
```

### Row Selection

Single selected row, highlighted. Click to select. Arrow keys navigate up/down.
Page Up / Page Down scroll by one screen. Home / End jump to first / last row.
Selected row drives the annotation panel and status bar.

Multiple rows can be selected in the listing. Right-click the selection and
choose `Create Segment from Selection` to open the segment editor prefilled
with the selected address span.

### Colour Coding — Mnemonics

Simple colour grouping by mnemonic string. No flow classification required —
these are static string matches applied when rendering each row.

| Group | Colour | Mnemonics |
|-------|--------|-----------|
| Branch / call | Orange | `JP`, `JR`, `CALL`, `RST`, `DJNZ` |
| Return | Muted orange | `RET`, `RETI`, `RETN` |
| Load | Blue | `LD` |
| Stack | Light blue | `PUSH`, `POP` |
| Arithmetic / logic | Amber | `ADD`, `ADC`, `SUB`, `SBC`, `AND`, `OR`, `XOR`, `CP`, `INC`, `DEC` |
| Exchange | Green | `EX`, `EXX` |
| Block ops | Teal | `LDIR`, `LDDR`, `CPIR`, `CPDR`, `INIR`, `INDR`, `OTIR`, `OTDR` |
| Halt | Red | `HALT` |
| Default | Primary text | All others (`NOP`, `DI`, `EI`, `IN`, `OUT`, etc.) |

### Colour Coding — Operands

Operand tokens are coloured when they resolve to a known symbol from
`symbols.sym`:

| Symbol type | Colour |
|------------|--------|
| `ROM_CALL` | Orange |
| `VECTOR` | Yellow |
| `JUMP_LABEL` | Green |
| `WRITABLE` | Cyan |
| `PORT` | Magenta |
| `CONSTANT` | Default text |

### Virtual Scrolling

A 16K ROM disassembles to several thousand rows; 64K produces ~20,000+. The
target design is a virtualised list — only visible rows are rendered. GTK4's
`GtkListView` with a `GListModel` is the end state. The current code still uses
`GtkListBox`, so treat this as the implementation target rather than the
present state.

---

## Right Panel Column

Fixed at 38.2% width. Four stacked panels separated by thin dividers, all
resizable via `GtkPaned`. In the current implementation, the segments,
symbols, and annotation panels are present; references remain planned.

### Segments Panel

Fixed height, no internal scroll (typical maps have 4–10 entries).
Panel header: `SEGMENTS` in small caps, muted.

Each row:
```
● NAME       0xSTART – 0xEND   [TYPE]
```

- Colour dot by type: ROM=green, RAM=blue, VRAM=teal, IO=magenta,
  SYSVARS=amber, UNMAPPED=grey
- Type badge: small coloured pill
- Clicking a row scrolls the listing to the start address of that region
- Double-clicking opens the Segment Entry Editor dialog

### Symbols Panel

Scrollable, takes flex space between Segments and Annotation panels.
Panel header: `SYMBOLS`. Own search/filter input at the top.

Each row:
```
ADDR    NAME                [TAG]
```

- ADDR: 4-digit hex, muted
- NAME: coloured by symbol type (matches operand colour coding above)
- TAG: small coloured pill badge
- Clicking scrolls the listing to that address and selects the row
- Double-clicking opens the Symbol Editor dialog

### References Panel

Planned. The current code does not ship this panel yet.

Scrollable, shares flex space with the Symbols panel.
Panel header: `REFERENCES`. Own search/filter input at the top.

Displays every unique `operand_addr` found across the entire `DisasmLine[]`
array — i.e. every address that is the target of a `JP`, `JR`, `CALL`, `RST`,
`LD rr,nn`, or any other instruction that names an address as an operand. Built
by a single O(n) pass over the line array after `disasm_range()` and
`symbols_resolve()` complete. Rebuilt automatically after every Regen.

Each row:

```
ADDR    TYPE        NAME          REFS    BYTES
0x0050  JUMP_LABEL  INIT            3     C3 50 00
0x1000  ROM_CALL    CLS             7     CD 00 10
0x4000  —           —               2     —
0x0123  ORPHAN      —               —     DB 22 ..
0x0187  ORPHAN      —               —     DD CB ..
```

| Column | Notes |
|--------|-------|
| ADDR | 4-digit hex, muted |
| TYPE | Symbol type tag if named (`ROM_CALL`, `JUMP_LABEL`, etc.); `ORPHAN` for decode errors; `—` for unnamed referenced addresses |
| NAME | Symbol or label name if one exists; `—` if unnamed |
| REFS | Count of instructions referencing this address; `—` for ORPHAN rows |
| BYTES | First 1–3 bytes from `rom.bin` if the address falls within the ROM range; `—` for RAM and I/O addresses |

**Unnamed `—` rows** are addresses being referenced by code that have not been
named yet — the primary discovery targets when reversing a ROM.

**ORPHAN rows** are bytes z80dasm could not decode in a CODE region — invalid
or ambiguous opcodes. They carry `RTYPE_ORPHAN` in the `DisasmLine` and are
collected by `xref_build()` separately from address references. Only the first
byte address is tracked. These rows render in a distinct warning colour (red /
magenta) so they stand out immediately.

Interactions:
- Clicking any row scrolls the listing to that address and selects the row
- Double-clicking an unnamed `—` row opens the Symbol Editor pre-filled with
  the address
- Double-clicking a named row opens the Symbol Editor to edit the existing entry
- Double-clicking an ORPHAN row scrolls to the address; the user can then mark
  the region as `DATA_BYTE` or investigate further
- Sorting: default by ADDR ascending; column header clicks toggle sort by REFS
  descending (most-referenced first), TYPE (groups ORPHANs together), or NAME
  alphabetically
- Filter input narrows by address, name, or type

The References panel updates immediately when a symbol is added or renamed —
no Regen required. ORPHAN rows only change after a Regen, since they come from
z80dasm output.

### Annotation Panel

Fixed height (~180px). Updates on every listing selection change.
Panel header: `ANNOTATION — 0xADDR` (address of selected row).

| Field | Notes |
|-------|-------|
| Label | Editable inline. Enter commits, Escape reverts. Label colour. |
| Inline comment | Editable inline. Comment colour. |
| Block | Multi-line editable text area. Comment colour. |
| Xref | Editable inline. Muted colour. |

Below the fields, a one-line metadata strip:
```
n bytes · REGION_TYPE
```

All four fields are editable directly in the panel. Changes are committed to the
in-memory `annotations.ann` model on Enter / focus-out, and written to disk on Save
or auto-save.

---

## Status Bar

Single row, bottom of window. Monospace, small, muted.
Left-aligned items, flex spacer, right-aligned project summary.

```
Offset: 0xADDR   Label: NAME   Region: TYPE   Bytes: N          PROJECT · load 0xADDR · SIZE bytes
```

Items update on row selection change. `Label` shows `—` when no label is set.

---

## Dialogs

### Segment Editor

Triggered by Add Segment button or by double-clicking a row's segment type indicator.

Fields:
- Start offset (hex input)
- End offset (hex input)
- Type (dropdown: CODE / DATA_BYTE / DATA_WORD / DATA_STRING / GAP)

Validates that the range does not overlap an existing region. Shows a warning
and highlights the conflicting region in the listing if it does.

### Symbol Editor

Triggered by Add Symbol button or double-clicking a symbol row.

Fields:
- Name (text input — assembler-safe identifier, validated)
- Address (hex input)
- Type (dropdown: ROM_CALL / VECTOR / JUMP_LABEL / WRITABLE / PORT / CONSTANT)

### Segment Entry Editor

Fields:
- Name (text input)
- Start address (hex input)
- End address (hex input)
- Type (dropdown: ROM / RAM / VRAM / IO / SYSVARS / Direct Byte Range / Direct Word Range / Define Message Range)

### Export Options

Shown before writing either export format.

- Output path (file chooser)
- Address range: Full ROM / Custom (start/end hex inputs)
- Hex style: uppercase / lowercase
- Include xref notes in output: yes/no

---

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| Ctrl+O | Open project |
| Ctrl+S | Save |
| Ctrl+R | Regen disassembly |
| Ctrl+F | Focus search bar |
| Escape | Clear search / cancel edit |
| ↑ / ↓ | Navigate listing rows |
| Page Up / Down | Scroll listing by screen |
| Home / End | Jump to first / last row |
| Enter | Edit annotation for selected row |
| Ctrl+E | Export .LST |
| Ctrl+Shift+E | Export .ASM |
| G | Go to address (hex input popup) |
| L | Add / edit label at selected address |
| C | Add / edit inline comment at selected address |
| B | Add / edit block comment at selected address |

---

## Fonts

- **Listing, status bar, symbol addresses:** monospace (GTK4 system default —
  typically JetBrains Mono or similar). 12px for listing rows, 11px for column
  headers and status bar.
- **Panel headers, toolbar, dialogs:** sans-serif system font. 11px for panel
  headers (small caps style), 13px for dialog labels.
- **Annotation text fields:** monospace 12px, to match the listing tone.

---

## Colours (Dark Mode — primary target)

Defined in `theme.css` loaded via `GtkCssProvider` on startup. All roles are
CSS custom properties so light mode can be provided by swapping the file.

| Role | Dark value | Light value |
|------|-----------|-------------|
| Background primary | `#1a1a1a` | `#ffffff` |
| Background secondary | `#242424` | `#f5f5f5` |
| Background tertiary | `#2e2e2e` | `#ebebeb` |
| Text primary | `#e8e8e8` | `#1a1a1a` |
| Text secondary | `#888` | `#555` |
| Text muted | `#555` | `#aaa` |
| Border | `#333` | `#d0d0d0` |
| Addr colour | `#85B7EB` | `#185FA5` |
| Comment colour | `#639922` | `#3B6D11` |
| Label colour | `#EF9F27` | `#854F0B` |
| Branch / call mnemonic | `#F0997B` | `#993C1D` |
| Load mnemonic | `#85B7EB` | `#185FA5` |
| Arith mnemonic | `#EF9F27` | `#854F0B` |
| Block op mnemonic | `#5DCAA5` | `#0F6E56` |
| Halt mnemonic | `#E24B4A` | `#A32D2D` |
| Selection bg | `#1e3a52` | `#dceeff` |
| Orphan row | `#E24B4A` | `#A32D2D` |

---

## GTK4 Widget Map

| UI element | GTK4 widget |
|-----------|-------------|
| Main window | `GtkApplicationWindow` |
| Toolbar | `GtkBox` with `GtkButton`, `GtkEntry` |
| Listing | `GtkListView` + `GListStore` + `GtkSignalListItemFactory` |
| Column header | `GtkBox` with fixed-width `GtkLabel` items |
| Right panel column | `GtkBox` vertical, `GtkPaned` for resizable splits |
| Segments | `GtkListBox` |
| Symbols | `GtkListBox` inside `GtkScrolledWindow` |
| Symbol filter | `GtkSearchEntry` |
| References | planned |
| References filter | `GtkSearchEntry` |
| Annotation panel | `GtkBox` with `GtkEntry` and `GtkTextView` fields |
| Status bar | `GtkBox` with `GtkLabel` items |
| Dialogs | `GtkDialog` / `GtkWindow` modal |
| File chooser | `GtkFileDialog` (GTK 4.10+) |

---

## Implementation Notes

- **References panel is a derived view.** `xref_build()` in `annotate.c` does
  a single O(n) pass over the `DisasmLine[]` array collecting two things: every
  unique `operand_addr` with its reference count (`XREF_ADDR`), and every
  `RTYPE_ORPHAN` line by its first-byte address (`XREF_ORPHAN`). No new file,
  no new parsing. Called once after every Regen and whenever the symbol table
  changes. Symbol additions and renames rebuild the panel instantly; ORPHAN rows
  only change after a Regen since they come from z80dasm output.
- **Virtual list is non-negotiable.** `GtkListView` with a `GListModel` is
  mandatory. Do not use `GtkTreeView` or append rows to a `GtkBox`.
- **Bytes column reads from rom.bin.** The UI populates `DisasmLine.bytes[]`
  from the ROM blob via `offset` + `byte_count`. It does not parse bytes from
  z80dasm output.
- **Mnemonic colouring is a static string table.** A simple array of
  `{mnemonic_string, colour_enum}` pairs. No flow analysis or opcode knowledge
  required — just `strcmp()` against the mnemonic field of each `DisasmLine`.
- **Annotation edits are in-memory first.** The `annotations.ann` in-memory
  structure is the source of truth during a session. File writes happen on Save.
  Auto-save writes only when the dirty flag is set.
- **Listing re-render on annotation change.** Editing a comment or label
  invalidates only the affected rows in the `GListModel`, not the full list.
- **Regen is async.** The z80dasm subprocess runs off the main thread. Show a
  progress indicator in the status bar (`Regenerating…`) and re-enable the UI
  when complete.
- **GTK4 CSS theming.** Load `theme.css` via `GtkCssProvider` on startup.
  Provide `theme-light.css` and `theme-dark.css`; select based on the system
  colour scheme preference.
