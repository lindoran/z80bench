# z80bench Status

This is the short, current-state note for the project.

## Works Today

- The project builds with `make`
- ROM projects can be opened and imported
- Session state persists across runs:
  - recent projects list
  - last-opened project path
  - auto-open-last preference
- The disassembly cache is regenerated from `rom.bin` and the active
  region model
- Segments, symbols, and annotations are editable in the GTK4 UI
- Segment model includes direct data ranges:
  - `DIRECT_BYTE` (`DEFB`)
  - `DIRECT_WORD` (`DEFW`)
  - `DEFINE_MSG` (`DEFM`)
- Operand text is normalized to `0x` style and can resolve to symbol/label names
- Side panel interactions are stateless:
  - single click jumps listing
  - double click opens edit dialog
- Segment and symbol dialogs both support immediate `Add`/`Update`/`Remove`
- Symbol edits now sync `JUMP_LABEL` entries back to listing/annotation labels
- Exporters for `.lst` and `.asm` are wired up
- Bottom hex dock supports visual filters for:
  - ASCII/text runs
  - high-bit byte runs
  - smart in-ROM / out-of-ROM address operand highlighting

## In Progress

- References panel workflow remains planned/not integrated

## Still Rough

- The listing view is functional, but it is not yet the virtualized
  `GtkListView` implementation described in the older spec
- The references panel is not fully implemented yet
- Segment dialog close behaviors differ:
  - `Close` button commits/reloads+jumps
  - window `X` follows normal destroy path
