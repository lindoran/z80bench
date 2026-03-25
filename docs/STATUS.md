# z80bench Status

This is the short, current-state note for the project.

## Works Today

- The project builds with `make`
- ROM projects can be opened and imported
- The disassembly cache is regenerated from `rom.bin` and the active
  region model
- Segments, symbols, and annotations are editable in the GTK4 UI
- The listing supports multi-select and a context menu for opening the segment
  editor from the selected address span
- Exporters for `.lst` and `.asm` are wired up

## In Progress

- The old "memory map" language is being renamed to "segments" in the UI and
  docs
- Direct-byte and define-message segments can now sit inside broader regions
- Removing one of those segments shows a warning because it does not rebuild
  the section workflow automatically

## Still Rough

- The listing view is functional, but it is not yet the virtualized
  `GtkListView` implementation described in the older spec
- The references panel is not fully implemented yet
- The docs were ahead of the code for a while, so some pages are still being
  brought back into sync
