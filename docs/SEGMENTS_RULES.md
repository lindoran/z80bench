# Segments Rules Groundwork

This is the implementation contract for segment behavior before tree UI work.

## Implementation Status (Current)

- Implemented in code:
  - Project-span validation on add/update:
    - `project_start = load_addr`
    - `project_end = load_addr + rom_size - 1`
    - all saved ranges must stay inside span
  - Parent overlap validation:
    - `ROM/RAM/VRAM/IO/SYSVARS` cannot overlap other parent ranges
  - Subrange overlap validation:
    - `DIRECT_BYTE/DIRECT_WORD/DEFINE_MSG` cannot overlap other subranges
  - Explicit `UNMAPPED` creation is blocked in dialog flow:
    - `UNMAPPED` is reserved/legacy and is not a user-authored saved range
  - Runtime grouping pass in current flat UI:
    - parent rows are sorted by memory address
    - parent rows are always visible and rendered with a leading `-`
    - subranges are grouped under containing parent rows
    - unowned subranges are shown directly (no `UNMAPPED` bucket)
- Still pending:
  - Full parent/subrange visual management UX polish in the panel

## 1) Segment classes

- Parent ranges:
  - `ROM`, `RAM`, `VRAM`, `IO`, `SYSVARS`
- Subranges:
  - `DIRECT_BYTE`, `DIRECT_WORD`, `DEFINE_MSG`
- `UNMAPPED`:
  - reserved for compatibility with older files; not exposed in editor flow

## 2) Address span

- Project span is:
  - `project_start = load_addr`
  - `project_end = load_addr + rom_size - 1`
- New/updated ranges must stay inside this span.

## 3) Invariants

1. Parent ranges cannot overlap other parent ranges.
2. Subranges cannot overlap other subranges.
3. Subranges are grouped under the containing parent in UI.
4. If a subrange is not inside a saved parent, it remains an unowned top-level
   row in the panel.

## 4) Ownership rule

- A subrange belongs to a parent if:
  - `parent.start <= sub.start && sub.end <= parent.end`
- Grouping is recomputed from containment each refresh.
- No explicit parent id is required in `segments.map`.

## 5) Persistence

- Keep `segments.map` flat for compatibility.
- Tree/group structure is runtime-derived, not serialized.

## 6) Initial project behavior

- New project starts with a default full-span `ROM` parent range (`ROM`).
- No derived `UNMAPPED` row is shown in panel UI.

## 7) UI direction

- Segments panel should evolve to a scrollable tree:
  - parent rows visible in memory order
  - subranges listed under parent
- Current UI may still show a flat list while validation/grouping groundwork
  is being completed.
