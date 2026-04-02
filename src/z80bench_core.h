#ifndef Z80BENCH_CORE_H
#define Z80BENCH_CORE_H

#include <stddef.h>
#include <stdio.h>

/*
 * z80bench_core — pure C API for project, disassembly, annotation,
 * symbol, map, and export functionality.
 */

/* --------------------------------------------------------------------------
 * Constants and Enums
 * -------------------------------------------------------------------------- */

#define Z80BENCH_ROM_FILE      "rom.bin"
#define Z80BENCH_MNM_FILE      "listing.mnm"
#define Z80BENCH_ANN_FILE      "annotations.ann"
#define Z80BENCH_MAP_FILE      "segments.map"
#define Z80BENCH_SYM_FILE      "symbols.sym"
#define Z80BENCH_APP_NAME      "z80bench"
#define Z80BENCH_VERSION       "1.0"
#define Z80BENCH_VERSION_LABEL "Version 1"
#define Z80BENCH_GITHUB_URL    "https://github.com/lindoran/z80bench"

#define DISASM_MNEMONIC_MAX   16
#define DISASM_OPERANDS_MAX   64
#define DISASM_LABEL_MAX      64
#define DISASM_COMMENT_MAX   1024
#define DISASM_BYTES_MAX       4    /* max Z80 instruction length */

typedef enum {
    RTYPE_CODE,
    RTYPE_DATA_BYTE,
    RTYPE_DATA_WORD,
    RTYPE_DATA_STRING,
    RTYPE_GAP,
    RTYPE_ORPHAN    /* byte z80dasm could not decode — invalid or ambiguous opcode */
} RegionType;

typedef enum {
    SYM_ROM_CALL,   /* Calls into ROM routines */
    SYM_VECTOR,     /* RST and interrupt vectors */
    SYM_JUMP_LABEL, /* Internal jump/call targets */
    SYM_WRITABLE,   /* RAM addresses */
    SYM_PORT,       /* I/O port numbers */
    SYM_CONSTANT    /* Pure numeric constants */
} SymbolType;

typedef enum {
    MAP_ROM,
    MAP_RAM,
    MAP_VRAM,
    MAP_IO,
    MAP_SYSVARS,
    MAP_UNMAPPED,
    MAP_DIRECT_BYTE,  /* byte range — renders as DEFB 0xNN,... */
    MAP_DIRECT_WORD,  /* word range — renders as DEFW 0xNNNN */
    MAP_DEFINE_MSG    /* string range — renders as DEFM "..." */
} MapType;

/* --------------------------------------------------------------------------
 * Data Structures
 * -------------------------------------------------------------------------- */

typedef struct {
    int           addr;                          /* absolute address (offset + load_addr) */
    int           offset;                        /* byte offset into project.bin */
    unsigned char bytes[DISASM_BYTES_MAX];       /* raw bytes from project.bin */
    int           byte_count;                    /* number of valid bytes */
    RegionType    rtype;                         /* CODE, DATA_*, or GAP */
    char          mnemonic[DISASM_MNEMONIC_MAX]; /* from z80dasm or data formatter */
    char          operands[DISASM_OPERANDS_MAX]; /* from z80dasm or data formatter */
    int           operand_addr;                  /* resolved address operand, -1 if none */

    /* populated by annotate_merge() after disasm_range() returns */
    char          label[DISASM_LABEL_MAX];       /* from project.ann [label] */
    char          comment[DISASM_COMMENT_MAX];   /* from project.ann [comment] */
    char          block[DISASM_COMMENT_MAX];     /* from project.ann [block] */

    /* populated by symbols_resolve() after disasm_range() returns */
    char          sym_name[DISASM_LABEL_MAX];    /* symbol name if operand_addr matches */
    int           sym_type;                      /* SymbolType enum value, -1 if none */
} DisasmLine;

typedef struct {
    int        offset;
    int        end;
    RegionType type;
} Region;

typedef struct {
    char       name[DISASM_LABEL_MAX];
    int        addr;
    SymbolType type;
    char       notes[DISASM_COMMENT_MAX];
} Symbol;

typedef struct {
    char    name[DISASM_LABEL_MAX];
    int     start;
    int     end;
    MapType type;
    char    notes[DISASM_COMMENT_MAX];
} MapEntry;

/* Forward declarations for internal data structures */
typedef struct AnnData AnnData;
typedef struct SymData SymData;
typedef struct MapData MapData;

typedef struct {
    char          name[256];
    int           load_addr;
    int           version;
    unsigned char *rom;
    int           rom_size;
    AnnData       *ann;
    SymData       *sym;
    MapData       *map;
    DisasmLine    *lines;
    int           nlines;
} Project;

/* --------------------------------------------------------------------------
 * Module: Project (project.c)
 * -------------------------------------------------------------------------- */

int project_import(Project *p, const char *bin_path, const char *proj_dir, int load_addr);
int project_open(Project *p, const char *path);
int project_save(Project *p, const char *path);
void project_close(Project *p);
void project_sync_map_to_regions(Project *p);
int project_summary_safe(const Project *p, char *name, size_t name_sz, int *load_addr,
                         int *rom_size, int *nlines, int *nsymbols, int *nmap_entries);

/* --------------------------------------------------------------------------
 * Module: Disassembly (disasm.c)
 * -------------------------------------------------------------------------- */

int disasm_range(
    const unsigned char *rom,
    int                  rom_size,
    int                  load_addr,
    int                  offset_start,
    int                  offset_end,
    const Region        *regions,
    int                  nregions,
    const char          *labelfile,
    DisasmLine          *out,
    int                  out_max
);

int disasm_write_mnm(const DisasmLine *lines, int nlines, const char *path);
int disasm_read_mnm(const char          *path,
                    const unsigned char *rom,
                    int                  load_addr,
                    DisasmLine          *out,
                    int                  out_max);

/* --------------------------------------------------------------------------
 * Module: Operand Context Helpers (opctx.c)
 * -------------------------------------------------------------------------- */

int opctx_has_absolute_operand_ref(const DisasmLine *dl);
int opctx_has_constant_immediate_operand(const DisasmLine *dl);
int opctx_prefers_byte_hex_width(const DisasmLine *dl);
int opctx_compute_operand_value(const DisasmLine *dl);

/* --------------------------------------------------------------------------
 * Module: Annotations (annotate.c)
 * -------------------------------------------------------------------------- */

AnnData *annotate_new(const char *name, int load_addr);
AnnData *annotate_load(const char *path);
int annotate_save(const AnnData *ann, const char *path);
void annotate_free(AnnData *ann);
void annotate_get_meta_safe(const AnnData *ann, char *name, size_t name_sz,
                            int *load_addr);
const Region *annotate_get_regions(const AnnData *ann, int *nregions);
void annotate_set_regions(AnnData *ann, const Region *regions, int nregions);
void annotate_merge(DisasmLine *lines, int nlines, const AnnData *ann);
void annotate_sync_from_lines(AnnData *ann, const DisasmLine *lines, int nlines);

/* --------------------------------------------------------------------------
 * Module: Symbols (symbols.c)
 * -------------------------------------------------------------------------- */

SymData *symbols_new(void);
SymData *symbols_load(const char *path);
int      symbols_save(const SymData *sym, const char *path);
void     symbols_free(SymData *sym);
void     symbols_resolve(DisasmLine *lines, int nlines, const SymData *sym);
void     symbols_format_operands(const DisasmLine *dl, char *out, size_t out_sz);
int      symbols_count(const SymData *sym);
const Symbol *symbols_get(const SymData *sym, int i);
int      symbols_get_safe(const SymData *sym, int i, Symbol *out);
int      symbols_add(SymData *sym, const Symbol *s);
int      symbols_remove(SymData *sym, int i);
int      symbols_replace(SymData *sym, int i, const Symbol *s);

/* --------------------------------------------------------------------------
 * Module: Segments / Map (memmap.c)
 * -------------------------------------------------------------------------- */

MapData *memmap_new(void);
MapData *memmap_load(const char *path);
int      memmap_save(const MapData *map, const char *path);
void     memmap_free(MapData *map);
int      memmap_count(const MapData *map);
const MapEntry *memmap_get(const MapData *map, int i);
int      memmap_get_safe(const MapData *map, int i, MapEntry *out);
int      memmap_add(MapData *map, const MapEntry *e);
int      memmap_remove(MapData *map, int i);
int      memmap_replace(MapData *map, int i, const MapEntry *e);

/* --------------------------------------------------------------------------
 * Module: Export (export.c)
 * -------------------------------------------------------------------------- */

int export_lst(const Project *p, const char *path);
int export_asm(const Project *p, const char *path);

#endif /* Z80BENCH_CORE_H */
