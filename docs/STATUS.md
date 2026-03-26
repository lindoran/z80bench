# z80bench Status

This is the short, current-state note for the project.

## Works Today

- The project builds with `make`
- ROM projects can be opened and imported
- The disassembly cache is regenerated from `rom.bin` and the active
  region model
- Segments, symbols, and annotations are editable in the GTK4 UI
- Side panel interactions are stateless:
  - single click jumps listing
  - double click opens edit dialog
- Segment and symbol dialogs both support immediate `Add`/`Update`/`Remove`
- Symbol edits now sync `JUMP_LABEL` entries back to listing/annotation labels
- Exporters for `.lst` and `.asm` are wired up

## In Progress

- Docs are being aligned to actual implemented behavior (not older target spec)
- References panel workflow remains planned/not integrated

## Still Rough

- The listing view is functional, but it is not yet the virtualized
  `GtkListView` implementation described in the older spec
- The references panel is not fully implemented yet
- Segment dialog close behaviors differ:
  - `Close` button commits/reloads+jumps
  - window `X` follows normal destroy path
