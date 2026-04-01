#include "z80bench_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_MAP_ENTRIES 100

struct MapData {
    MapEntry entries[MAX_MAP_ENTRIES];
    int nentries;
};

static void copy_text(char *dst, size_t dst_sz, const char *src) {
    if (!dst || dst_sz == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, dst_sz, "%s", src);
}

MapData *memmap_load(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return NULL;

    MapData *map = calloc(1, sizeof(MapData));
    if (!map) {
        fclose(fp);
        return NULL;
    }

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

        /* Strip trailing newline */
        char *nl = strchr(line, '\n'); if (nl) *nl = '\0';
        nl = strchr(line, '\r'); if (nl) *nl = '\0';

        /* Tab-delimited: start\tend\ttype\tname\tnotes
         * Legacy space-delimited (old format): name start end type notes
         * Detect by checking if first non-space char is '0' (new) or alpha (old) */
        char start_str[32], end_str[32], type_str[32];
        char name_buf[DISASM_LABEL_MAX] = "";
        char notes_buf[DISASM_COMMENT_MAX] = "";

        if (line[0] == '0') {
            /* New tab-delimited format */
            char *f1 = line;
            char *f2 = strchr(f1, '\t'); if (!f2) continue; *f2++ = '\0';
            char *f3 = strchr(f2, '\t'); if (!f3) continue; *f3++ = '\0';
            char *f4 = strchr(f3, '\t'); if (!f4) continue; *f4++ = '\0';
            char *f5 = strchr(f4, '\t'); if (f5) { *f5++ = '\0'; copy_text(notes_buf, sizeof(notes_buf), f5); }

            copy_text(start_str, sizeof(start_str), f1);
            copy_text(end_str,   sizeof(end_str), f2);
            copy_text(type_str,  sizeof(type_str), f3);
            copy_text(name_buf,  sizeof(name_buf), f4);
        } else {
            /* Legacy space-delimited: name start end type */
            char tmp_name[DISASM_LABEL_MAX];
            if (sscanf(line, "%63s %31s %31s %31s", tmp_name, start_str, end_str, type_str) != 4) continue;
            copy_text(name_buf, sizeof(name_buf), tmp_name);
        }

        if (map->nentries < MAX_MAP_ENTRIES) {
            MapEntry *e = &map->entries[map->nentries++];
            copy_text(e->name,  sizeof(e->name),  name_buf);
            copy_text(e->notes, sizeof(e->notes), notes_buf);

            e->start = (int)strtol(start_str, NULL, 0);
            e->end   = (int)strtol(end_str,   NULL, 0);

            if      (strcmp(type_str, "ROM")         == 0) e->type = MAP_ROM;
            else if (strcmp(type_str, "RAM")         == 0) e->type = MAP_RAM;
            else if (strcmp(type_str, "VRAM")        == 0) e->type = MAP_VRAM;
            else if (strcmp(type_str, "IO")          == 0) e->type = MAP_IO;
            else if (strcmp(type_str, "SYSVARS")     == 0) e->type = MAP_SYSVARS;
            else if (strcmp(type_str, "DIRECT_BYTE") == 0) e->type = MAP_DIRECT_BYTE;
            else if (strcmp(type_str, "DIRECT_WORD") == 0) e->type = MAP_DIRECT_WORD;
            else if (strcmp(type_str, "DEFINE_MSG")  == 0) e->type = MAP_DEFINE_MSG;
            else                                            e->type = MAP_UNMAPPED;
        }
    }

    fclose(fp);
    return map;
}

int memmap_save(const MapData *map, const char *path) {
    if (!map) return -1;
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;

    fprintf(fp, "# z80bench segment map\n");
    fprintf(fp, "# format: start<TAB>end<TAB>type<TAB>name<TAB>notes\n\n");

    for (int i = 0; i < map->nentries; i++) {
        const MapEntry *e = &map->entries[i];
        const char *type_str = "UNMAPPED";
        switch (e->type) {
            case MAP_ROM:         type_str = "ROM"; break;
            case MAP_RAM:         type_str = "RAM"; break;
            case MAP_VRAM:        type_str = "VRAM"; break;
            case MAP_IO:          type_str = "IO"; break;
            case MAP_SYSVARS:     type_str = "SYSVARS"; break;
            case MAP_DIRECT_BYTE: type_str = "DIRECT_BYTE"; break;
            case MAP_DIRECT_WORD: type_str = "DIRECT_WORD"; break;
            case MAP_DEFINE_MSG:  type_str = "DEFINE_MSG"; break;
            default: type_str = "UNMAPPED"; break;
        }
        fprintf(fp, "0x%04X\t0x%04X\t%s\t%s\t%s\n",
                e->start, e->end, type_str, e->name, e->notes);
    }

    fclose(fp);
    return 0;
}

void memmap_free(MapData *map) {
    free(map);
}

MapData *memmap_new(void) { return calloc(1, sizeof(MapData)); }

int memmap_count(const MapData *map) { return map ? map->nentries : 0; }

const MapEntry *memmap_get(const MapData *map, int i) {
    if (!map || i < 0 || i >= map->nentries) return NULL;
    return &map->entries[i];
}

int memmap_add(MapData *map, const MapEntry *e) {
    if (!map || !e || map->nentries >= MAX_MAP_ENTRIES) return -1;
    map->entries[map->nentries++] = *e;
    return 0;
}

int memmap_remove(MapData *map, int i) {
    if (!map || i < 0 || i >= map->nentries) return -1;
    memmove(&map->entries[i], &map->entries[i + 1],
            (map->nentries - i - 1) * sizeof(MapEntry));
    map->nentries--;
    return 0;
}

int memmap_replace(MapData *map, int i, const MapEntry *e) {
    if (!map || !e || i < 0 || i >= map->nentries) return -1;
    map->entries[i] = *e;
    return 0;
}

int memmap_get_safe(const MapData *map, int i, MapEntry *out) {
    if (!map || !out || i < 0 || i >= map->nentries) return -1;
    *out = map->entries[i];
    return 0;
}
