#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include "z80bench_core.h"

static const char *map_type_name(MapType t) {
    switch (t) {
        case MAP_ROM:         return "ROM";
        case MAP_RAM:         return "RAM";
        case MAP_VRAM:        return "VRAM";
        case MAP_IO:          return "IO";
        case MAP_SYSVARS:     return "SYSVARS";
        case MAP_UNMAPPED:    return "UNMAPPED";
        case MAP_DIRECT_BYTE: return "DIRECT_BYTE";
        case MAP_DEFINE_MSG:  return "DEFINE_MSG";
        default:              return "UNKNOWN";
    }
}

static const char *region_type_name(RegionType t) {
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

static int validate_map(const Project *p) {
    int errors = 0;
    int nmap = memmap_count(p->map);

    for (int i = 0; i < nmap; i++) {
        MapEntry a;
        if (memmap_get_safe(p->map, i, &a) != 0) continue;
        if (a.start > a.end) {
            fprintf(stderr, "MAP_ERR index=%d start_gt_end start=0x%04X end=0x%04X\n",
                    i, a.start, a.end);
            errors++;
        }
        if (a.type == MAP_DIRECT_BYTE || a.type == MAP_DEFINE_MSG) {
            if (!a.name[0]) {
                fprintf(stderr, "MAP_ERR index=%d empty_name type=%s\n",
                        i, map_type_name(a.type));
                errors++;
            }
        }
        for (int j = i + 1; j < nmap; j++) {
            MapEntry b;
            if (memmap_get_safe(p->map, j, &b) != 0) continue;
            if (a.type != MAP_DIRECT_BYTE && a.type != MAP_DEFINE_MSG) continue;
            if (b.type != MAP_DIRECT_BYTE && b.type != MAP_DEFINE_MSG) continue;
            if (a.start <= b.end && b.start <= a.end) {
                fprintf(stderr,
                        "MAP_ERR overlap a=%d b=%d a=[0x%04X-0x%04X] b=[0x%04X-0x%04X]\n",
                        i, j, a.start, a.end, b.start, b.end);
                errors++;
            }
        }
    }

    return errors;
}

static void dump_project(const Project *p) {
    char name[sizeof(p->name)];
    int load_addr = 0, rom_size = 0, nlines = 0, nsymbols = 0, nmap = 0;
    if (project_summary_safe(p, name, sizeof(name), &load_addr, &rom_size,
                             &nlines, &nsymbols, &nmap) != 0) {
        printf("PROJECT summary unavailable\n");
        return;
    }
    printf("PROJECT name=%s load=0x%04X rom_size=%d lines=%d symbols=%d map=%d\n",
           name, load_addr, rom_size, nlines, nsymbols, nmap);
}

static void dump_map(const Project *p) {
    int nmap = memmap_count(p->map);
    for (int i = 0; i < nmap; i++) {
        MapEntry e;
        if (memmap_get_safe(p->map, i, &e) != 0) continue;
        printf("SEGMENT index=%d start=0x%04X end=0x%04X type=%s name=%s notes=%s\n",
               i, e.start, e.end, map_type_name(e.type), e.name, e.notes);
    }
}

static void dump_regions(const Project *p) {
    int nregions = 0;
    annotate_get_regions(p->ann, &nregions);
    for (int i = 0; i < nregions; i++) {
        Region region;
        if (annotate_get_region_safe(p->ann, i, &region) != 0) continue;
        printf("REGION index=%d offset=%d end=%d type=%s\n",
               i, region.offset, region.end, region_type_name(region.type));
    }
}

static void usage(const char *argv0) {
    fprintf(stderr,
            "Usage:\n"
            "  %s inspect <project_dir>\n"
            "  %s validate <project_dir>\n"
            "  %s regions <project_dir>\n"
            "  %s migrate <project_dir>\n",
            argv0, argv0, argv0, argv0);
}

static int path_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static void print_layout(const char *proj) {
    char path[1024];
    const char *files[] = {
        Z80BENCH_ROM_FILE,
        Z80BENCH_MNM_FILE,
        Z80BENCH_ANN_FILE,
        Z80BENCH_MAP_FILE,
        Z80BENCH_SYM_FILE,
        "project.bin",
        "project.mnm",
        "project.ann",
        "project.map",
        "project.sym",
        NULL
    };
    for (int i = 0; files[i]; i++) {
        snprintf(path, sizeof(path), "%s/%s", proj, files[i]);
        printf("FILE %-16s %s\n", files[i], path_exists(path) ? "present" : "missing");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        usage(argv[0]);
        return 2;
    }

    const char *cmd = argv[1];
    const char *proj = argv[2];

    Project p;
    memset(&p, 0, sizeof(p));
    if (project_open(&p, proj) != 0) {
        fprintf(stderr, "Failed to open project: %s (errno=%d: %s)\n",
                proj, errno, strerror(errno));
        return 1;
    }

    int rc = 0;

    if (strcmp(cmd, "inspect") == 0) {
        dump_project(&p);
        dump_map(&p);
    } else if (strcmp(cmd, "validate") == 0) {
        dump_project(&p);
        rc = validate_map(&p);
        if (rc == 0)
            printf("VALIDATION OK\n");
    } else if (strcmp(cmd, "regions") == 0) {
        dump_project(&p);
        dump_regions(&p);
    } else if (strcmp(cmd, "migrate") == 0) {
        dump_project(&p);
        if (project_save(&p, proj) != 0) {
            fprintf(stderr, "MIGRATION save failed\n");
            rc = 1;
        } else {
            printf("MIGRATION OK\n");
            print_layout(proj);
        }
    } else {
        usage(argv[0]);
        rc = 2;
    }

    project_close(&p);
    return rc ? 1 : 0;
}
