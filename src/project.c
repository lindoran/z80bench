#include "z80bench_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#define MAX_TOTAL_LINES 100000

static const char *const project_bin_files[] = {
    Z80BENCH_ROM_FILE,
    "project.bin",
    NULL
};
static const char *const project_ann_files[] = {
    Z80BENCH_ANN_FILE,
    "project.ann",
    NULL
};
static const char *const project_sym_files[] = {
    Z80BENCH_SYM_FILE,
    "project.sym",
    NULL
};
static const char *const project_map_files[] = {
    Z80BENCH_MAP_FILE,
    "project.map",
    NULL
};

static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static void join_path(char *out, size_t out_sz, const char *dir, const char *name) {
    if (!out || out_sz == 0) return;
    snprintf(out, out_sz, "%s/%s", dir, name);
}

static int resolve_project_file(const char *dir, const char *const *names,
                                char *out, size_t out_sz) {
    for (int i = 0; names[i]; i++) {
        join_path(out, out_sz, dir, names[i]);
        if (file_exists(out))
            return 0;
    }
    return -1;
}

static int write_empty_text_file(const char *path, const char *header1, const char *header2) {
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;
    if (header1) fprintf(fp, "%s\n", header1);
    if (header2) fprintf(fp, "%s\n", header2);
    fclose(fp);
    return 0;
}

static void remove_legacy_file(const char *dir, const char *new_path,
                               const char *legacy_name) {
    char old_path[1024];
    join_path(old_path, sizeof(old_path), dir, legacy_name);
    if (strcmp(old_path, new_path) != 0)
        remove(old_path);
}

static int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return -1;
    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }
    unsigned char buf[4096];
    size_t nread;
    while ((nread = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, nread, out) != nread) {
            fclose(in);
            fclose(out);
            return -1;
        }
    }
    fclose(in);
    fclose(out);
    return 0;
}

static int region_cmp(const void *a, const void *b) {
    const Region *ra = a;
    const Region *rb = b;
    if (ra->offset != rb->offset) return ra->offset - rb->offset;
    return ra->end - rb->end;
}

static void region_append(Region *regions, int *nregions,
                          int offset, int end, RegionType type) {
    if (offset > end || *nregions >= 100) return;
    regions[*nregions].offset = offset;
    regions[*nregions].end    = end;
    regions[*nregions].type   = type;
    (*nregions)++;
}

static void region_subtract_segment(Region *regions, int *nregions,
                                    int load_addr, const MapEntry *seg) {
    Region out[100];
    int nout = 0;
    int seg_start = seg->start - load_addr;
    int seg_end   = seg->end   - load_addr;

    for (int i = 0; i < *nregions && nout < 100; i++) {
        const Region *src = &regions[i];

        if (src->end < seg_start || src->offset > seg_end) {
            out[nout++] = *src;
            continue;
        }

        if (src->offset < seg_start)
            region_append(out, &nout, src->offset, seg_start - 1, src->type);
        if (src->end > seg_end)
            region_append(out, &nout, seg_end + 1, src->end, src->type);
    }

    memcpy(regions, out, sizeof(Region) * nout);
    *nregions = nout;
}

int project_import(Project *p, const char *bin_path, const char *proj_dir, int load_addr) {
    /* Create project directory */
    if (mkdir(proj_dir, 0755) != 0 && errno != EEXIST) return -1;

    /* Copy bin as project.bin — skip if already there */
    char dest[1024];
    join_path(dest, sizeof(dest), proj_dir, Z80BENCH_ROM_FILE);
    if (!file_exists(dest)) {
        FILE *src_fp = fopen(bin_path, "rb");
        if (!src_fp) return -1;
        FILE *dst_fp = fopen(dest, "wb");
        if (!dst_fp) { fclose(src_fp); return -1; }
        unsigned char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), src_fp)) > 0)
            fwrite(buf, 1, n, dst_fp);
        fclose(src_fp);
        fclose(dst_fp);
    }

    /* Write a minimal annotations file with the user-supplied load address */
    char ann_path[1024];
    join_path(ann_path, sizeof(ann_path), proj_dir, Z80BENCH_ANN_FILE);
    if (!file_exists(ann_path)) {
        const char *name = strrchr(proj_dir, '/');
        name = name ? name + 1 : proj_dir;
        FILE *fp = fopen(ann_path, "w");
        if (fp) {
            fprintf(fp, "# z80bench annotation file\n\n");
            fprintf(fp, "[meta]\nname = %s\nload_addr = 0x%04X\n\n", name, (unsigned)load_addr);
            fclose(fp);
        }
    }

    /* Create empty companion files so the folder is self-describing. */
    char sym_path[1024];
    char map_path[1024];
    join_path(sym_path, sizeof(sym_path), proj_dir, Z80BENCH_SYM_FILE);
    join_path(map_path, sizeof(map_path), proj_dir, Z80BENCH_MAP_FILE);
    if (!file_exists(sym_path))
        write_empty_text_file(sym_path,
                              "; z80bench symbol file",
                              "; assembler: z88dk/z80asm");
    if (!file_exists(map_path))
        write_empty_text_file(map_path,
                              "# z80bench segment map",
                              "# format: start<TAB>end<TAB>type<TAB>name<TAB>notes");

    return project_open(p, proj_dir);
}

int project_open(Project *p, const char *path) {
    memset(p, 0, sizeof(Project));

    char filename[512];
    
    /* 1. Load project.bin */
    if (resolve_project_file(path, project_bin_files, filename, sizeof(filename)) != 0)
        return -1;
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -1;
    fseek(fp, 0, SEEK_END);
    p->rom_size = ftell(fp);
    if (p->rom_size <= 0) {
        fclose(fp);
        return -1;
    }
    fseek(fp, 0, SEEK_SET);
    p->rom = malloc(p->rom_size);
    if (!p->rom) {
        fclose(fp);
        return -1;
    }
    if (fread(p->rom, 1, p->rom_size, fp) != (size_t)p->rom_size) {
        fclose(fp);
        free(p->rom);
        p->rom = NULL;
        return -1;
    }
    fclose(fp);

    /* 2. Load project.ann */
    if (resolve_project_file(path, project_ann_files, filename, sizeof(filename)) != 0)
        return -1;
    p->ann = annotate_load(filename);
    if (p->ann) {
        annotate_get_meta_safe(p->ann, p->name, sizeof(p->name), &p->load_addr);
    } else {
        /* No .ann file yet — allocate an empty one so saves work */
        const char *base = strrchr(path, '/');
        base = base ? base + 1 : path;
        p->ann = annotate_new(base, p->load_addr);
        if (!p->ann) {
            project_close(p);
            return -1;
        }
    }

    /* 3. Load project.sym */
    if (resolve_project_file(path, project_sym_files, filename, sizeof(filename)) == 0)
        p->sym = symbols_load(filename);

    /* 4. Load project.map */
    if (resolve_project_file(path, project_map_files, filename, sizeof(filename)) == 0)
        p->map = memmap_load(filename);

    /* Segments drive the region model, so fold them in before the first
     * disassembly pass. This lets direct-byte / define-message ranges sit
     * inside broader code regions. */
    project_sync_map_to_regions(p);

    /* 5. Regen project.mnm from source — always regenerate so stale caches
     * never cause display corruption. .mnm is a derived file, not a source. */
    p->lines = calloc(MAX_TOTAL_LINES, sizeof(DisasmLine));
    if (!p->lines) {
        project_close(p);
        return -1;
    }
    join_path(filename, sizeof(filename), path, Z80BENCH_MNM_FILE);
    remove_legacy_file(path, filename, "project.mnm");
    p->nlines = 0;      /* force regen path */

    if (p->nlines <= 0) {
        /* Regen */
        int nregions;
        const Region *regions = annotate_get_regions(p->ann, &nregions);
        p->nlines = disasm_range(p->rom, p->rom_size, p->load_addr, 0, p->rom_size - 1,
                                 regions, nregions, NULL, p->lines, MAX_TOTAL_LINES);
        if (p->nlines > 0) {
            disasm_write_mnm(p->lines, p->nlines, filename);
        }
    }

    /* 6. Initial merge */
    if (p->nlines > 0) {
        annotate_merge(p->lines, p->nlines, p->ann);
        symbols_resolve(p->lines, p->nlines, p->sym);
    }

    return 0;
}

int project_summary_safe(const Project *p, char *name, size_t name_sz, int *load_addr,
                         int *rom_size, int *nlines, int *nsymbols, int *nmap_entries) {
    if (!p) return -1;
    if (name && name_sz > 0) snprintf(name, name_sz, "%s", p->name);
    if (load_addr) *load_addr = p->load_addr;
    if (rom_size) *rom_size = p->rom_size;
    if (nlines) *nlines = p->nlines;
    if (nsymbols) *nsymbols = symbols_count(p->sym);
    if (nmap_entries) *nmap_entries = memmap_count(p->map);
    return 0;
}

void project_sync_map_to_regions(Project *p) {
    if (!p->ann || !p->map) return;

    /* Get the current region list */
    int nreg;
    const Region *old = annotate_get_regions(p->ann, &nreg);

    /* Build a new list:
     * - keep the existing non-segment-driven regions
     * - carve any segment ranges out of them
     * - add fresh ranges from direct data map entries (byte/word/message)
     *
     * This keeps direct-byte / string segments nestable inside broader CODE
     * regions instead of forcing the user to split them manually. */
    Region new_regions[100];
    int nnew = 0;

    /* Copy regions that are not segment-driven, trimming out any nested
     * segment ranges as we go. */
    for (int i = 0; i < nreg && nnew < 99; i++) {
        if (old[i].type == RTYPE_DATA_BYTE ||
            old[i].type == RTYPE_DATA_WORD ||
            old[i].type == RTYPE_DATA_STRING)
            continue;

        Region trimmed[100];
        int ntrimmed = 0;
        trimmed[ntrimmed++] = old[i];

        for (int m = 0; m < memmap_count(p->map); m++) {
            const MapEntry *me = memmap_get(p->map, m);
            if (me->type != MAP_DIRECT_BYTE &&
                me->type != MAP_DIRECT_WORD &&
                me->type != MAP_DEFINE_MSG) continue;
            region_subtract_segment(trimmed, &ntrimmed, p->load_addr, me);
            if (ntrimmed == 0) break;
        }

        for (int t = 0; t < ntrimmed && nnew < 99; t++)
            new_regions[nnew++] = trimmed[t];
    }

    /* Add regions from direct data map entries */
    for (int m = 0; m < memmap_count(p->map) && nnew < 99; m++) {
        const MapEntry *me = memmap_get(p->map, m);
        if (me->type != MAP_DIRECT_BYTE &&
            me->type != MAP_DIRECT_WORD &&
            me->type != MAP_DEFINE_MSG) continue;
        new_regions[nnew].offset = me->start - p->load_addr;
        new_regions[nnew].end    = me->end   - p->load_addr;
        if (me->type == MAP_DEFINE_MSG)
            new_regions[nnew].type = RTYPE_DATA_STRING;
        else if (me->type == MAP_DIRECT_WORD)
            new_regions[nnew].type = RTYPE_DATA_WORD;
        else
            new_regions[nnew].type = RTYPE_DATA_BYTE;
        nnew++;
    }

    qsort(new_regions, nnew, sizeof(Region), region_cmp);

    annotate_set_regions(p->ann, new_regions, nnew);
}

int project_save(Project *p, const char *path) {
    char filename[512];
    char legacy_rom[1024];
    char new_rom[1024];

    join_path(new_rom, sizeof(new_rom), path, Z80BENCH_ROM_FILE);
    join_path(legacy_rom, sizeof(legacy_rom), path, "project.bin");
    if (!file_exists(new_rom) && file_exists(legacy_rom)) {
        if (rename(legacy_rom, new_rom) != 0) {
            if (copy_file(legacy_rom, new_rom) == 0)
                remove(legacy_rom);
        }
    } else if (file_exists(new_rom) && file_exists(legacy_rom)) {
        remove(legacy_rom);
    }

    if (p->ann && p->map)
        project_sync_map_to_regions(p);

    if (p->ann && p->lines && p->nlines > 0) {
        /* Sync live DisasmLine edits back into AnnData before writing */
        annotate_sync_from_lines(p->ann, p->lines, p->nlines);
    }

    if (p->ann) {
        join_path(filename, sizeof(filename), path, Z80BENCH_ANN_FILE);
        annotate_save(p->ann, filename);
        remove_legacy_file(path, filename, "project.ann");
    } else {
        join_path(filename, sizeof(filename), path, Z80BENCH_ANN_FILE);
        remove_legacy_file(path, filename, "project.ann");
    }
    if (p->sym) {
        join_path(filename, sizeof(filename), path, Z80BENCH_SYM_FILE);
        symbols_save(p->sym, filename);
        remove_legacy_file(path, filename, "project.sym");
    } else {
        join_path(filename, sizeof(filename), path, Z80BENCH_SYM_FILE);
        write_empty_text_file(filename,
                              "; z80bench symbol file",
                              "; assembler: z88dk/z80asm");
        remove_legacy_file(path, filename, "project.sym");
    }
    if (p->map) {
        join_path(filename, sizeof(filename), path, Z80BENCH_MAP_FILE);
        memmap_save(p->map, filename);
        remove_legacy_file(path, filename, "project.map");
    } else {
        join_path(filename, sizeof(filename), path, Z80BENCH_MAP_FILE);
        write_empty_text_file(filename,
                              "# z80bench segment map",
                              "# format: start<TAB>end<TAB>type<TAB>name<TAB>notes");
        remove_legacy_file(path, filename, "project.map");
    }
    /* listing.mnm is derived, but we might want to save it if nlines changed */
    if (p->nlines > 0) {
        join_path(filename, sizeof(filename), path, Z80BENCH_MNM_FILE);
        disasm_write_mnm(p->lines, p->nlines, filename);
        remove_legacy_file(path, filename, "project.mnm");
    } else {
        join_path(filename, sizeof(filename), path, Z80BENCH_MNM_FILE);
        remove_legacy_file(path, filename, "project.mnm");
    }

    return 0;
}

void project_close(Project *p) {
    if (p->rom) free(p->rom);
    if (p->ann) annotate_free(p->ann);
    if (p->sym) symbols_free(p->sym);
    if (p->map) memmap_free(p->map);
    if (p->lines) free(p->lines);
    memset(p, 0, sizeof(Project));
}
