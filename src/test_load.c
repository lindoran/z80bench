/*
 * z80bench-cli — scriptable / AI-agent interface for z80bench projects.
 *
 * Output format: JSON-lines (one JSON object per stdout line).
 * Errors go to stderr as {"error":"...","cmd":"..."}.
 *
 * Usage:
 *   z80bench-cli <command> [args...]
 *   z80bench-cli batch <project_dir>         # read commands from stdin
 *   z80bench-cli help                        # list all commands
 *
 * Commands:
 *   project create  <dir> <rom.bin> [load_addr_hex]
 *   project info    <dir>
 *   project save    <dir>
 *   project disasm  <dir>
 *   project export  <dir> lst|asm [outfile]
 *
 *   segment list    <dir>
 *   segment add     <dir> <start_hex> <end_hex> <type> <name> [notes]
 *   segment remove  <dir> <index>
 *   segment update  <dir> <index> <start_hex> <end_hex> <type> <name> [notes]
 *
 *   symbol list     <dir>
 *   symbol add      <dir> <name> <addr_hex> <type> [notes]
 *   symbol remove   <dir> <name>
 *   symbol update   <dir> <name> <addr_hex> <type> [notes]
 *
 *   annotation get  <dir> <addr_hex>
 *   annotation set  <dir> <addr_hex> <field> <value>
 *     field: label | comment | block
 *   annotation list <dir>
 *   annotation list-range <dir> <from_hex> <to_hex>
 *
 *   line get        <dir> <addr_hex>
 *   line list       <dir> [from_hex [to_hex]]
 *
 * Types for segment: ROM RAM VRAM IO SYSVARS UNMAPPED DIRECT_BYTE DIRECT_WORD DEFINE_MSG
 * Types for symbol:  ROM_CALL VECTOR JUMP_LABEL WRITABLE PORT CONSTANT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include "z80bench_core.h"

/* ============================================================
 * JSON helpers
 * ============================================================ */

static void json_escape(FILE *fp, const char *s) {
    fputc('"', fp);
    for (; *s; s++) {
        if      (*s == '"')  fputs("\\\"", fp);
        else if (*s == '\\') fputs("\\\\", fp);
        else if (*s == '\n') fputs("\\n",  fp);
        else if (*s == '\r') fputs("\\r",  fp);
        else if (*s == '\t') fputs("\\t",  fp);
        else fputc(*s, fp);
    }
    fputc('"', fp);
}

static void ok(const char *cmd) {
    printf("{\"ok\":true,\"cmd\":");
    json_escape(stdout, cmd);
    printf("}\n");
    fflush(stdout);
}

static void err(const char *cmd, const char *msg) {
    fprintf(stderr, "{\"error\":");
    json_escape(stderr, msg);
    fprintf(stderr, ",\"cmd\":");
    json_escape(stderr, cmd);
    fprintf(stderr, "}\n");
    fflush(stderr);
}

/* ============================================================
 * Type name maps
 * ============================================================ */

static const char *map_type_str(MapType t) {
    switch (t) {
        case MAP_ROM:         return "ROM";
        case MAP_RAM:         return "RAM";
        case MAP_VRAM:        return "VRAM";
        case MAP_IO:          return "IO";
        case MAP_SYSVARS:     return "SYSVARS";
        case MAP_UNMAPPED:    return "UNMAPPED";
        case MAP_DIRECT_BYTE: return "DIRECT_BYTE";
        case MAP_DIRECT_WORD: return "DIRECT_WORD";
        case MAP_DEFINE_MSG:  return "DEFINE_MSG";
        default:              return "UNKNOWN";
    }
}

static MapType map_type_from_str(const char *s) {
    if (!s) return MAP_UNMAPPED;
    if (strcmp(s,"ROM")==0)         return MAP_ROM;
    if (strcmp(s,"RAM")==0)         return MAP_RAM;
    if (strcmp(s,"VRAM")==0)        return MAP_VRAM;
    if (strcmp(s,"IO")==0)          return MAP_IO;
    if (strcmp(s,"SYSVARS")==0)     return MAP_SYSVARS;
    if (strcmp(s,"DIRECT_BYTE")==0) return MAP_DIRECT_BYTE;
    if (strcmp(s,"DIRECT_WORD")==0) return MAP_DIRECT_WORD;
    if (strcmp(s,"DEFINE_MSG")==0)  return MAP_DEFINE_MSG;
    return MAP_UNMAPPED;
}

static const char *sym_type_str(SymbolType t) {
    switch (t) {
        case SYM_ROM_CALL:   return "ROM_CALL";
        case SYM_VECTOR:     return "VECTOR";
        case SYM_JUMP_LABEL: return "JUMP_LABEL";
        case SYM_WRITABLE:   return "WRITABLE";
        case SYM_PORT:       return "PORT";
        case SYM_CONSTANT:   return "CONSTANT";
        default:             return "UNKNOWN";
    }
}

static SymbolType sym_type_from_str(const char *s) {
    if (!s) return SYM_CONSTANT;
    if (strcmp(s,"ROM_CALL")==0)   return SYM_ROM_CALL;
    if (strcmp(s,"VECTOR")==0)     return SYM_VECTOR;
    if (strcmp(s,"JUMP_LABEL")==0) return SYM_JUMP_LABEL;
    if (strcmp(s,"WRITABLE")==0)   return SYM_WRITABLE;
    if (strcmp(s,"PORT")==0)       return SYM_PORT;
    return SYM_CONSTANT;
}

static const char *rtype_str(RegionType t) {
    switch (t) {
        case RTYPE_CODE:        return "CODE";
        case RTYPE_DATA_BYTE:   return "DATA_BYTE";
        case RTYPE_DATA_WORD:   return "DATA_WORD";
        case RTYPE_DATA_STRING: return "DATA_STRING";
        case RTYPE_GAP:         return "GAP";
        case RTYPE_ORPHAN:      return "ORPHAN";
        default:                return "UNKNOWN";
    }
}

/* ============================================================
 * Project open/save helpers
 * ============================================================ */

static int open_project(Project *p, const char *dir, const char *cmd) {
    memset(p, 0, sizeof(*p));
    if (project_open(p, dir) != 0) {
        char msg[512];
        snprintf(msg, sizeof(msg), "cannot open project '%s': %s", dir, strerror(errno));
        err(cmd, msg);
        return -1;
    }
    return 0;
}

static int save_project(Project *p, const char *dir, const char *cmd) {
    if (project_save(p, dir) != 0) {
        err(cmd, "project_save failed");
        return -1;
    }
    return 0;
}

/* ============================================================
 * Command handlers
 * ============================================================ */

/* project create <dir> <rom.bin> [load_addr_hex] */
static int cmd_project_create(int argc, char **argv) {
    if (argc < 5) { err("project create", "usage: project create <dir> <rom.bin> [load_addr]"); return 1; }
    const char *dir     = argv[3];
    const char *romfile = argv[4];
    int load_addr = (argc >= 6) ? (int)strtol(argv[5], NULL, 0) : 0;

    Project p;
    if (project_import(&p, romfile, dir, load_addr) != 0) {
        err("project create", "project_import failed");
        return 1;
    }
    char name[256]; int la, rs, nl, ns, nm;
    project_summary_safe(&p, name, sizeof(name), &la, &rs, &nl, &ns, &nm);
    printf("{\"ok\":true,\"cmd\":\"project create\",\"name\":");
    json_escape(stdout, name);
    printf(",\"dir\":");
    json_escape(stdout, dir);
    printf(",\"load_addr\":\"0x%04X\",\"rom_size\":%d,\"lines\":%d}\n", la, rs, nl);
    fflush(stdout);
    project_close(&p);
    return 0;
}

/* project info <dir> */
static int cmd_project_info(int argc, char **argv) {
    if (argc < 4) { err("project info","usage: project info <dir>"); return 1; }
    Project p;
    if (open_project(&p, argv[3], "project info") != 0) return 1;
    char name[256]; int la, rs, nl, ns, nm;
    project_summary_safe(&p, name, sizeof(name), &la, &rs, &nl, &ns, &nm);
    printf("{\"ok\":true,\"cmd\":\"project info\",\"name\":");
    json_escape(stdout, name);
    printf(",\"dir\":");
    json_escape(stdout, argv[3]);
    printf(",\"load_addr\":\"0x%04X\",\"rom_size\":%d,\"lines\":%d,\"symbols\":%d,\"segments\":%d}\n",
           la, rs, nl, ns, nm);
    fflush(stdout);
    project_close(&p);
    return 0;
}

/* project save <dir> */
static int cmd_project_save(int argc, char **argv) {
    if (argc < 4) { err("project save","usage: project save <dir>"); return 1; }
    Project p;
    if (open_project(&p, argv[3], "project save") != 0) return 1;
    int r = save_project(&p, argv[3], "project save");
    if (r == 0) ok("project save");
    project_close(&p);
    return r ? 1 : 0;
}

/* project disasm <dir> */
static int cmd_project_disasm(int argc, char **argv) {
    if (argc < 4) { err("project disasm","usage: project disasm <dir>"); return 1; }
    Project p;
    if (open_project(&p, argv[3], "project disasm") != 0) return 1;
    project_sync_map_to_regions(&p);
    int nregions; const Region *regions = annotate_get_regions(p.ann, &nregions);
    p.nlines = disasm_range(p.rom, p.rom_size, p.load_addr, 0, p.rom_size-1,
                            regions, nregions, NULL, p.lines, 100000);
    if (p.nlines > 0) {
        annotate_merge(p.lines, p.nlines, p.ann);
        symbols_resolve(p.lines, p.nlines, p.sym);
    }
    save_project(&p, argv[3], "project disasm");
    printf("{\"ok\":true,\"cmd\":\"project disasm\",\"lines\":%d}\n", p.nlines);
    fflush(stdout);
    project_close(&p);
    return 0;
}

/* project export <dir> lst|asm [outfile] */
static int cmd_project_export(int argc, char **argv) {
    if (argc < 5) { err("project export","usage: project export <dir> lst|asm [outfile]"); return 1; }
    const char *dir = argv[3];
    const char *fmt = argv[4];
    char outpath[1024];
    if (argc >= 6) {
        snprintf(outpath, sizeof(outpath), "%s", argv[5]);
    } else {
        snprintf(outpath, sizeof(outpath), "%s/export.%s", dir, fmt);
    }
    Project p;
    if (open_project(&p, dir, "project export") != 0) return 1;
    int r = 0;
    if (strcmp(fmt,"lst")==0)      r = export_lst(&p, outpath);
    else if (strcmp(fmt,"asm")==0) r = export_asm(&p, outpath);
    else { err("project export","format must be lst or asm"); project_close(&p); return 1; }
    if (r != 0) { err("project export","export failed"); project_close(&p); return 1; }
    printf("{\"ok\":true,\"cmd\":\"project export\",\"format\":");
    json_escape(stdout, fmt);
    printf(",\"file\":");
    json_escape(stdout, outpath);
    printf("}\n");
    fflush(stdout);
    project_close(&p);
    return 0;
}

/* segment list <dir> */
static int cmd_segment_list(int argc, char **argv) {
    if (argc < 4) { err("segment list","usage: segment list <dir>"); return 1; }
    Project p; if (open_project(&p, argv[3], "segment list") != 0) return 1;
    int n = memmap_count(p.map);
    printf("{\"ok\":true,\"cmd\":\"segment list\",\"count\":%d,\"segments\":[", n);
    for (int i = 0; i < n; i++) {
        MapEntry e; if (memmap_get_safe(p.map, i, &e) != 0) continue;
        if (i) printf(",");
        printf("{\"index\":%d,\"start\":\"0x%04X\",\"end\":\"0x%04X\",\"type\":\"%s\",\"name\":",
               i, e.start, e.end, map_type_str(e.type));
        json_escape(stdout, e.name);
        printf(",\"notes\":"); json_escape(stdout, e.notes);
        printf("}");
    }
    printf("]}\n"); fflush(stdout);
    project_close(&p); return 0;
}

/* segment add <dir> <start> <end> <type> <name> [notes] */
static int cmd_segment_add(int argc, char **argv) {
    if (argc < 8) { err("segment add","usage: segment add <dir> <start> <end> <type> <name> [notes]"); return 1; }
    Project p; if (open_project(&p, argv[3], "segment add") != 0) return 1;
    MapEntry e = {0};
    e.start = (int)strtol(argv[4], NULL, 0);
    e.end   = (int)strtol(argv[5], NULL, 0);
    e.type  = map_type_from_str(argv[6]);
    strncpy(e.name,  argv[7], DISASM_LABEL_MAX-1);
    if (argc >= 9) strncpy(e.notes, argv[8], DISASM_COMMENT_MAX-1);
    if (!p.map) p.map = memmap_new();
    int idx = memmap_count(p.map);
    memmap_add(p.map, &e);
    if (e.type == MAP_DIRECT_BYTE || e.type == MAP_DIRECT_WORD || e.type == MAP_DEFINE_MSG) {
        project_sync_map_to_regions(&p);
        /* Re-disassemble */
        int nregions; const Region *regions = annotate_get_regions(p.ann, &nregions);
        p.nlines = disasm_range(p.rom, p.rom_size, p.load_addr, 0, p.rom_size-1,
                                regions, nregions, NULL, p.lines, 100000);
        if (p.nlines > 0) {
            annotate_merge(p.lines, p.nlines, p.ann);
            symbols_resolve(p.lines, p.nlines, p.sym);
        }
    }
    save_project(&p, argv[3], "segment add");
    printf("{\"ok\":true,\"cmd\":\"segment add\",\"index\":%d}\n", idx);
    fflush(stdout);
    project_close(&p); return 0;
}

/* segment remove <dir> <index> */
static int cmd_segment_remove(int argc, char **argv) {
    if (argc < 5) { err("segment remove","usage: segment remove <dir> <index>"); return 1; }
    Project p; if (open_project(&p, argv[3], "segment remove") != 0) return 1;
    int idx = atoi(argv[4]);
    MapEntry e = {0}; memmap_get_safe(p.map, idx, &e);
    if (memmap_remove(p.map, idx) != 0) { err("segment remove","invalid index"); project_close(&p); return 1; }
    if (e.type == MAP_DIRECT_BYTE || e.type == MAP_DIRECT_WORD || e.type == MAP_DEFINE_MSG) {
        project_sync_map_to_regions(&p);
        int nregions; const Region *regions = annotate_get_regions(p.ann, &nregions);
        p.nlines = disasm_range(p.rom, p.rom_size, p.load_addr, 0, p.rom_size-1,
                                regions, nregions, NULL, p.lines, 100000);
        if (p.nlines > 0) {
            annotate_merge(p.lines, p.nlines, p.ann);
            symbols_resolve(p.lines, p.nlines, p.sym);
        }
    }
    save_project(&p, argv[3], "segment remove");
    printf("{\"ok\":true,\"cmd\":\"segment remove\",\"index\":%d}\n", idx);
    fflush(stdout);
    project_close(&p); return 0;
}

/* segment update <dir> <index> <start> <end> <type> <name> [notes] */
static int cmd_segment_update(int argc, char **argv) {
    if (argc < 9) { err("segment update","usage: segment update <dir> <index> <start> <end> <type> <name> [notes]"); return 1; }
    Project p; if (open_project(&p, argv[3], "segment update") != 0) return 1;
    int idx = atoi(argv[4]);
    MapEntry e = {0};
    e.start = (int)strtol(argv[5], NULL, 0);
    e.end   = (int)strtol(argv[6], NULL, 0);
    e.type  = map_type_from_str(argv[7]);
    strncpy(e.name, argv[8], DISASM_LABEL_MAX-1);
    if (argc >= 10) strncpy(e.notes, argv[9], DISASM_COMMENT_MAX-1);
    if (memmap_replace(p.map, idx, &e) != 0) { err("segment update","invalid index"); project_close(&p); return 1; }
    project_sync_map_to_regions(&p);
    int nregions; const Region *regions = annotate_get_regions(p.ann, &nregions);
    p.nlines = disasm_range(p.rom, p.rom_size, p.load_addr, 0, p.rom_size-1,
                            regions, nregions, NULL, p.lines, 100000);
    if (p.nlines > 0) { annotate_merge(p.lines, p.nlines, p.ann); symbols_resolve(p.lines, p.nlines, p.sym); }
    save_project(&p, argv[3], "segment update");
    printf("{\"ok\":true,\"cmd\":\"segment update\",\"index\":%d}\n", idx);
    fflush(stdout);
    project_close(&p); return 0;
}

/* symbol list <dir> */
static int cmd_symbol_list(int argc, char **argv) {
    if (argc < 4) { err("symbol list","usage: symbol list <dir>"); return 1; }
    Project p; if (open_project(&p, argv[3], "symbol list") != 0) return 1;
    int n = symbols_count(p.sym);
    printf("{\"ok\":true,\"cmd\":\"symbol list\",\"count\":%d,\"symbols\":[", n);
    for (int i = 0; i < n; i++) {
        Symbol s; if (symbols_get_safe(p.sym, i, &s) != 0) continue;
        if (i) printf(",");
        printf("{\"index\":%d,\"name\":", i); json_escape(stdout, s.name);
        printf(",\"addr\":\"0x%04X\",\"type\":\"%s\",\"notes\":", s.addr, sym_type_str(s.type));
        json_escape(stdout, s.notes); printf("}");
    }
    printf("]}\n"); fflush(stdout);
    project_close(&p); return 0;
}

/* symbol add <dir> <name> <addr> <type> [notes] */
static int cmd_symbol_add(int argc, char **argv) {
    if (argc < 7) { err("symbol add","usage: symbol add <dir> <name> <addr> <type> [notes]"); return 1; }
    Project p; if (open_project(&p, argv[3], "symbol add") != 0) return 1;
    if (!p.sym) p.sym = symbols_new();
    Symbol s = {0};
    strncpy(s.name, argv[4], DISASM_LABEL_MAX-1);
    s.addr = (int)strtol(argv[5], NULL, 0);
    s.type = sym_type_from_str(argv[6]);
    if (argc >= 8) strncpy(s.notes, argv[7], DISASM_COMMENT_MAX-1);
    int idx = symbols_count(p.sym);
    symbols_add(p.sym, &s);
    save_project(&p, argv[3], "symbol add");
    printf("{\"ok\":true,\"cmd\":\"symbol add\",\"index\":%d}\n", idx);
    fflush(stdout);
    project_close(&p); return 0;
}

/* symbol remove <dir> <name> */
static int cmd_symbol_remove(int argc, char **argv) {
    if (argc < 5) { err("symbol remove","usage: symbol remove <dir> <name>"); return 1; }
    Project p; if (open_project(&p, argv[3], "symbol remove") != 0) return 1;
    const char *name = argv[4];
    int found = -1;
    for (int i = 0; i < symbols_count(p.sym); i++) {
        Symbol s; symbols_get_safe(p.sym, i, &s);
        if (strcmp(s.name, name) == 0) { found = i; break; }
    }
    if (found < 0) { err("symbol remove","symbol not found"); project_close(&p); return 1; }
    symbols_remove(p.sym, found);
    save_project(&p, argv[3], "symbol remove");
    printf("{\"ok\":true,\"cmd\":\"symbol remove\",\"index\":%d}\n", found);
    fflush(stdout);
    project_close(&p); return 0;
}

/* symbol update <dir> <name> <new_addr> <new_type> [new_notes] */
static int cmd_symbol_update(int argc, char **argv) {
    if (argc < 7) { err("symbol update","usage: symbol update <dir> <name> <addr> <type> [notes]"); return 1; }
    Project p; if (open_project(&p, argv[3], "symbol update") != 0) return 1;
    int found = -1;
    for (int i = 0; i < symbols_count(p.sym); i++) {
        Symbol s; symbols_get_safe(p.sym, i, &s);
        if (strcmp(s.name, argv[4]) == 0) { found = i; break; }
    }
    if (found < 0) { err("symbol update","symbol not found"); project_close(&p); return 1; }
    Symbol s = {0};
    strncpy(s.name, argv[4], DISASM_LABEL_MAX-1);
    s.addr = (int)strtol(argv[5], NULL, 0);
    s.type = sym_type_from_str(argv[6]);
    if (argc >= 8) strncpy(s.notes, argv[7], DISASM_COMMENT_MAX-1);
    symbols_replace(p.sym, found, &s);
    save_project(&p, argv[3], "symbol update");
    printf("{\"ok\":true,\"cmd\":\"symbol update\",\"index\":%d}\n", found);
    fflush(stdout);
    project_close(&p); return 0;
}

/* annotation get <dir> <addr> */
static int cmd_annotation_get(int argc, char **argv) {
    if (argc < 5) { err("annotation get","usage: annotation get <dir> <addr>"); return 1; }
    Project p; if (open_project(&p, argv[3], "annotation get") != 0) return 1;
    int addr = (int)strtol(argv[4], NULL, 0);
    for (int i = 0; i < p.nlines; i++) {
        DisasmLine *dl = &p.lines[i];
        if (dl->addr == addr) {
            printf("{\"ok\":true,\"cmd\":\"annotation get\",\"addr\":\"0x%04X\",\"label\":", addr);
            json_escape(stdout, dl->label);
            printf(",\"comment\":"); json_escape(stdout, dl->comment);
            printf(",\"block\":");   json_escape(stdout, dl->block);
            printf(",\"mnemonic\":"); json_escape(stdout, dl->mnemonic);
            printf(",\"operands\":"); json_escape(stdout, dl->operands);
            printf(",\"rtype\":\"%s\"}\n", rtype_str(dl->rtype));
            fflush(stdout);
            project_close(&p); return 0;
        }
    }
    err("annotation get","address not found in listing");
    project_close(&p); return 1;
}

/* annotation set <dir> <addr> <field> <value> */
static int cmd_annotation_set(int argc, char **argv) {
    if (argc < 7) { err("annotation set","usage: annotation set <dir> <addr> <field> <value>"); return 1; }
    Project p; if (open_project(&p, argv[3], "annotation set") != 0) return 1;
    int addr = (int)strtol(argv[4], NULL, 0);
    const char *field = argv[5];
    const char *value = argv[6];
    int found = 0;
    for (int i = 0; i < p.nlines; i++) {
        DisasmLine *dl = &p.lines[i];
        if (dl->addr != addr) continue;
        found = 1;
        if      (strcmp(field,"label")==0)   strncpy(dl->label,   value, DISASM_LABEL_MAX-1);
        else if (strcmp(field,"comment")==0) strncpy(dl->comment, value, DISASM_COMMENT_MAX-1);
        else if (strcmp(field,"block")==0)   strncpy(dl->block,   value, DISASM_COMMENT_MAX-1);
        else { err("annotation set","field must be label|comment|block"); project_close(&p); return 1; }
        break;
    }
    if (!found) { err("annotation set","address not found in listing"); project_close(&p); return 1; }
    save_project(&p, argv[3], "annotation set");
    printf("{\"ok\":true,\"cmd\":\"annotation set\",\"addr\":\"0x%04X\",\"field\":", addr);
    json_escape(stdout, field);
    printf("}\n"); fflush(stdout);
    project_close(&p); return 0;
}

/* annotation list <dir> */
/* annotation list-range <dir> <from> <to> */
static int cmd_annotation_list(int argc, char **argv, int range_mode) {
    const char *cmd = range_mode ? "annotation list-range" : "annotation list";
    if (argc < 4) { err(cmd,"usage: annotation list[-range] <dir> [from to]"); return 1; }
    int from_addr = 0, to_addr = 0x10000;
    if (range_mode) {
        if (argc < 6) { err(cmd,"usage: annotation list-range <dir> <from> <to>"); return 1; }
        from_addr = (int)strtol(argv[4], NULL, 0);
        to_addr   = (int)strtol(argv[5], NULL, 0);
    }
    Project p; if (open_project(&p, argv[3], cmd) != 0) return 1;
    printf("{\"ok\":true,\"cmd\":\"%s\",\"annotations\":[", cmd);
    int first = 1;
    for (int i = 0; i < p.nlines; i++) {
        DisasmLine *dl = &p.lines[i];
        if (dl->addr < from_addr || dl->addr > to_addr) continue;
        if (!dl->label[0] && !dl->comment[0] && !dl->block[0]) continue;
        if (!first) printf(",");
        first = 0;
        printf("{\"addr\":\"0x%04X\",\"label\":", dl->addr);
        json_escape(stdout, dl->label);
        printf(",\"comment\":"); json_escape(stdout, dl->comment);
        printf(",\"block\":");   json_escape(stdout, dl->block);
        printf("}");
    }
    printf("]}\n"); fflush(stdout);
    project_close(&p); return 0;
}

/* line get <dir> <addr> */
static int cmd_line_get(int argc, char **argv) {
    if (argc < 5) { err("line get","usage: line get <dir> <addr>"); return 1; }
    Project p; if (open_project(&p, argv[3], "line get") != 0) return 1;
    int addr = (int)strtol(argv[4], NULL, 0);
    for (int i = 0; i < p.nlines; i++) {
        DisasmLine *dl = &p.lines[i];
        if (dl->addr != addr) continue;
        /* build hex bytes string */
        char hex[32] = "";
        for (int b = 0; b < dl->byte_count && b < 4; b++) { char t[3]; sprintf(t,"%02X",dl->bytes[b]); strcat(hex,t); }
        printf("{\"ok\":true,\"cmd\":\"line get\",\"addr\":\"0x%04X\",\"offset\":%d,"
               "\"bytes\":\"%s\",\"byte_count\":%d,\"rtype\":\"%s\","
               "\"mnemonic\":", dl->addr, dl->offset, hex, dl->byte_count, rtype_str(dl->rtype));
        json_escape(stdout, dl->mnemonic);
        printf(",\"operands\":"); json_escape(stdout, dl->operands);
        printf(",\"label\":");    json_escape(stdout, dl->label);
        printf(",\"comment\":");  json_escape(stdout, dl->comment);
        printf(",\"block\":");    json_escape(stdout, dl->block);
        printf("}\n"); fflush(stdout);
        project_close(&p); return 0;
    }
    err("line get","address not found");
    project_close(&p); return 1;
}

/* line list <dir> [from [to]] */
static int cmd_line_list(int argc, char **argv) {
    if (argc < 4) { err("line list","usage: line list <dir> [from [to]]"); return 1; }
    int from_addr = 0, to_addr = 0x10000;
    if (argc >= 5) from_addr = (int)strtol(argv[4], NULL, 0);
    if (argc >= 6) to_addr   = (int)strtol(argv[5], NULL, 0);
    Project p; if (open_project(&p, argv[3], "line list") != 0) return 1;
    printf("{\"ok\":true,\"cmd\":\"line list\",\"lines\":[");
    int first = 1;
    for (int i = 0; i < p.nlines; i++) {
        DisasmLine *dl = &p.lines[i];
        if (dl->addr < from_addr || dl->addr > to_addr) continue;
        if (!first) printf(",");
        first = 0;
        char hex[32] = "";
        for (int b = 0; b < dl->byte_count && b < 4; b++) { char t[3]; sprintf(t,"%02X",dl->bytes[b]); strcat(hex,t); }
        printf("{\"addr\":\"0x%04X\",\"bytes\":\"%s\",\"rtype\":\"%s\",\"mnemonic\":",
               dl->addr, hex, rtype_str(dl->rtype));
        json_escape(stdout, dl->mnemonic);
        printf(",\"operands\":"); json_escape(stdout, dl->operands);
        if (dl->label[0])   { printf(",\"label\":");   json_escape(stdout, dl->label); }
        if (dl->comment[0]) { printf(",\"comment\":"); json_escape(stdout, dl->comment); }
        printf("}");
    }
    printf("]}\n"); fflush(stdout);
    project_close(&p); return 0;
}

/* ============================================================
 * Batch / stdin mode
 * ============================================================ */

static int dispatch(int argc, char **argv);

static int cmd_batch(int argc, char **argv) {
    /* Read lines from stdin, split on whitespace, dispatch as sub-commands.
     * Prefix each sub-command with the project dir so callers don't repeat it. */
    if (argc < 3) { err("batch","usage: batch <project_dir>"); return 1; }
    const char *dir = argv[2];
    char line[4096];
    int rc = 0;
    while (fgets(line, sizeof(line), stdin)) {
        /* Strip newline */
        int len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
        if (!len || line[0] == '#') continue;

        /* Tokenise */
        char *tok[64]; int ntok = 0;
        tok[ntok++] = argv[0]; /* argv[0] = program name */
        char *p = line;
        while (*p && ntok < 63) {
            while (*p == ' ' || *p == '\t') p++;
            if (!*p) break;
            if (*p == '"') {
                /* quoted token */
                p++;
                tok[ntok++] = p;
                while (*p && *p != '"') p++;
                if (*p == '"') *p++ = '\0';
            } else {
                tok[ntok++] = p;
                while (*p && *p != ' ' && *p != '\t') p++;
                if (*p) *p++ = '\0';
            }
        }
        if (ntok < 2) continue;

        /* Insert dir after sub-verb (argv[3]) */
        char *full[66];
        full[0] = tok[0];
        full[1] = tok[1];
        full[2] = (ntok >= 3) ? tok[2] : "";
        full[3] = (char *)dir;
        int fntok = ntok + 1;
        for (int i = 3; i < ntok; i++) full[i+1] = tok[i];

        int r = dispatch(fntok, full);
        if (r != 0) rc = r;
    }
    return rc;
}

/* ============================================================
 * Help
 * ============================================================ */

static void print_help(void) {
    printf("z80bench-cli %s — AI/script interface for z80bench projects\n\n",
           Z80BENCH_VERSION_LABEL);
    printf("Output: JSON-lines on stdout. Errors: JSON on stderr.\n\n");
    printf("Commands:\n");
    printf("  project create  <dir> <rom.bin> [load_addr]\n");
    printf("  project info    <dir>\n");
    printf("  project save    <dir>\n");
    printf("  project disasm  <dir>          # re-run disassembly\n");
    printf("  project export  <dir> lst|asm [outfile]\n\n");
    printf("  segment list    <dir>\n");
    printf("  segment add     <dir> <start> <end> <type> <name> [notes]\n");
    printf("  segment remove  <dir> <index>\n");
    printf("  segment update  <dir> <index> <start> <end> <type> <name> [notes]\n");
    printf("    types: ROM RAM VRAM IO SYSVARS UNMAPPED DIRECT_BYTE DIRECT_WORD DEFINE_MSG\n\n");
    printf("  symbol list     <dir>\n");
    printf("  symbol add      <dir> <name> <addr> <type> [notes]\n");
    printf("  symbol remove   <dir> <name>\n");
    printf("  symbol update   <dir> <name> <addr> <type> [notes]\n");
    printf("    types: ROM_CALL VECTOR JUMP_LABEL WRITABLE PORT CONSTANT\n\n");
    printf("  annotation get  <dir> <addr>\n");
    printf("  annotation set  <dir> <addr> label|comment|block <value>\n");
    printf("  annotation list <dir>\n");
    printf("  annotation list-range <dir> <from> <to>\n\n");
    printf("  line get        <dir> <addr>\n");
    printf("  line list       <dir> [from [to]]\n\n");
    printf("  batch           <dir>          # read commands from stdin\n");
    printf("  help\n\n");
    printf("Addresses: decimal or 0x hex. Batch lines omit the project dir.\n");
}

/* ============================================================
 * Dispatch
 * ============================================================ */

static int dispatch(int argc, char **argv) {
    if (argc < 2) { print_help(); return 0; }
    const char *verb = argv[1];
    const char *sub  = (argc >= 3) ? argv[2] : "";

    if (strcmp(verb,"help")==0 || strcmp(verb,"--help")==0 || strcmp(verb,"-h")==0) {
        print_help(); return 0;
    }

    /* Two-word commands need one more shift */
    if (strcmp(verb,"project")==0) {
        if      (strcmp(sub,"create")==0)  return cmd_project_create(argc, argv);
        else if (strcmp(sub,"info")==0)    return cmd_project_info(argc, argv);
        else if (strcmp(sub,"save")==0)    return cmd_project_save(argc, argv);
        else if (strcmp(sub,"disasm")==0)  return cmd_project_disasm(argc, argv);
        else if (strcmp(sub,"export")==0)  return cmd_project_export(argc, argv);
        else { err("project","unknown sub-command"); return 1; }
    }
    if (strcmp(verb,"segment")==0) {
        if      (strcmp(sub,"list")==0)    return cmd_segment_list(argc, argv);
        else if (strcmp(sub,"add")==0)     return cmd_segment_add(argc, argv);
        else if (strcmp(sub,"remove")==0)  return cmd_segment_remove(argc, argv);
        else if (strcmp(sub,"update")==0)  return cmd_segment_update(argc, argv);
        else { err("segment","unknown sub-command"); return 1; }
    }
    if (strcmp(verb,"symbol")==0) {
        if      (strcmp(sub,"list")==0)    return cmd_symbol_list(argc, argv);
        else if (strcmp(sub,"add")==0)     return cmd_symbol_add(argc, argv);
        else if (strcmp(sub,"remove")==0)  return cmd_symbol_remove(argc, argv);
        else if (strcmp(sub,"update")==0)  return cmd_symbol_update(argc, argv);
        else { err("symbol","unknown sub-command"); return 1; }
    }
    if (strcmp(verb,"annotation")==0) {
        if      (strcmp(sub,"get")==0)          return cmd_annotation_get(argc, argv);
        else if (strcmp(sub,"set")==0)          return cmd_annotation_set(argc, argv);
        else if (strcmp(sub,"list")==0)         return cmd_annotation_list(argc, argv, 0);
        else if (strcmp(sub,"list-range")==0)   return cmd_annotation_list(argc, argv, 1);
        else { err("annotation","unknown sub-command"); return 1; }
    }
    if (strcmp(verb,"line")==0) {
        if      (strcmp(sub,"get")==0)     return cmd_line_get(argc, argv);
        else if (strcmp(sub,"list")==0)    return cmd_line_list(argc, argv);
        else { err("line","unknown sub-command"); return 1; }
    }
    if (strcmp(verb,"batch")==0) return cmd_batch(argc, argv);

    err(verb,"unknown command — try 'help'");
    return 1;
}

int main(int argc, char **argv) {
    return dispatch(argc, argv);
}
