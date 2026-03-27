#include "z80bench_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_REGIONS 100
#define MAX_ANNOTATIONS 1000

static void copy_text(char *dst, size_t dst_sz, const char *src) {
    if (!dst || dst_sz == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, dst_sz, "%s", src);
}

static void unescape_newlines(char *s) {
    char *d = s;
    while (*s) {
        if (*s == '\\' && *(s+1) == 'n') {
            *d++ = '\n';
            s += 2;
        } else {
            *d++ = *s++;
        }
    }
    *d = '\0';
}

static void trim_trailing_newlines(char *s) {
    if (!s) return;
    int len = strlen(s);
    while (len > 0 && (s[len-1] == '\n' || s[len-1] == '\r')) {
        s[--len] = '\0';
    }
}

typedef struct {
    int offset;
    char name[DISASM_LABEL_MAX];
} Label;

typedef struct {
    int offset;
    char text[DISASM_COMMENT_MAX];
} Comment;

struct AnnData {
    char name[256];
    int  load_addr;

    Region regions[MAX_REGIONS];
    int nregions;
    
    Label labels[MAX_ANNOTATIONS];
    int nlabels;
    
    Comment comments[MAX_ANNOTATIONS];
    int ncomments;
    
    Comment blocks[MAX_ANNOTATIONS];
    int nblocks;
    
    Comment xrefs[MAX_ANNOTATIONS];
    int nxrefs;
};

AnnData *annotate_load(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return NULL;

    AnnData *ann = calloc(1, sizeof(AnnData));
    if (!ann) {
        fclose(fp);
        return NULL;
    }

    char line[512];
    char section[64] = "";
    char *last_text = NULL;
    size_t last_text_sz = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == ';') {
            if (last_text) trim_trailing_newlines(last_text);
            last_text = NULL;
            continue;
        }

        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        nl = strchr(line, '\r');
        if (nl) *nl = '\0';

        if (line[0] == '[' && strchr(line, ']')) {
            if (last_text) trim_trailing_newlines(last_text);
            char *end = strchr(line, ']');
            int len = end - line - 1;
            if (len > 0 && len < (int)sizeof(section)) {
                strncpy(section, line + 1, len);
                section[len] = '\0';
            }
            last_text = NULL;
            continue;
        }

        char *eq = strchr(line, '=');
        if (eq) {
            if (last_text) trim_trailing_newlines(last_text);
            *eq = '\0';
            char *key = line;
            char *val = eq + 1;
            while (*key && isspace(*key)) key++;
            char *ke = key + strlen(key) - 1;
            while (ke > key && isspace(*ke)) { *ke = '\0'; ke--; }
            while (*val && isspace(*val)) val++;

            last_text = NULL;

            if (strcmp(section, "meta") == 0) {
                if (strcmp(key, "name") == 0) copy_text(ann->name, sizeof(ann->name), val);
                else if (strcmp(key, "load_addr") == 0) ann->load_addr = (int)strtol(val, NULL, 0);
            } else if (strcmp(section, "region") == 0) {
                if (strcmp(key, "offset") == 0) {
                    if (ann->nregions < MAX_REGIONS) ann->regions[ann->nregions].offset = atoi(val);
                } else if (strcmp(key, "end") == 0) {
                    if (ann->nregions < MAX_REGIONS) ann->regions[ann->nregions].end = atoi(val);
                } else if (strcmp(key, "type") == 0) {
                    if (ann->nregions < MAX_REGIONS) {
                        if (strcmp(val, "CODE") == 0) ann->regions[ann->nregions].type = RTYPE_CODE;
                        else if (strcmp(val, "DATA_BYTE") == 0) ann->regions[ann->nregions].type = RTYPE_DATA_BYTE;
                        else if (strcmp(val, "DATA_WORD") == 0) ann->regions[ann->nregions].type = RTYPE_DATA_WORD;
                        else if (strcmp(val, "DATA_STRING") == 0) ann->regions[ann->nregions].type = RTYPE_DATA_STRING;
                        else if (strcmp(val, "GAP") == 0) ann->regions[ann->nregions].type = RTYPE_GAP;
                        ann->nregions++;
                    }
                }
            } else if (strcmp(section, "label") == 0) {
                if (strcmp(key, "offset") == 0) {
                    if (ann->nlabels < MAX_ANNOTATIONS) ann->labels[ann->nlabels].offset = atoi(val);
                } else if (strcmp(key, "name") == 0) {
                    if (ann->nlabels < MAX_ANNOTATIONS) {
                        copy_text(ann->labels[ann->nlabels].name, sizeof(ann->labels[ann->nlabels].name), val);
                        ann->nlabels++;
                    }
                }
            } else if (strcmp(section, "comment") == 0) {
                if (strcmp(key, "offset") == 0) {
                    if (ann->ncomments < MAX_ANNOTATIONS) ann->comments[ann->ncomments].offset = atoi(val);
                } else if (strcmp(key, "text") == 0) {
                    if (ann->ncomments < MAX_ANNOTATIONS) {
                        unescape_newlines(val);
                        copy_text(ann->comments[ann->ncomments].text, sizeof(ann->comments[ann->ncomments].text), val);
                        last_text = ann->comments[ann->ncomments].text;
                        last_text_sz = sizeof(ann->comments[ann->ncomments].text);
                        ann->ncomments++;
                    }
                }
            } else if (strcmp(section, "block") == 0) {
                if (strcmp(key, "offset") == 0) {
                    if (ann->nblocks < MAX_ANNOTATIONS) ann->blocks[ann->nblocks].offset = atoi(val);
                } else if (strcmp(key, "text") == 0) {
                    if (ann->nblocks < MAX_ANNOTATIONS) {
                        unescape_newlines(val);
                        copy_text(ann->blocks[ann->nblocks].text, sizeof(ann->blocks[ann->nblocks].text), val);
                        last_text = ann->blocks[ann->nblocks].text;
                        last_text_sz = sizeof(ann->blocks[ann->nblocks].text);
                        ann->nblocks++;
                    }
                }
            } else if (strcmp(section, "xref") == 0) {
                if (strcmp(key, "offset") == 0) {
                    if (ann->nxrefs < MAX_ANNOTATIONS) ann->xrefs[ann->nxrefs].offset = atoi(val);
                } else if (strcmp(key, "text") == 0) {
                    if (ann->nxrefs < MAX_ANNOTATIONS) {
                        unescape_newlines(val);
                        copy_text(ann->xrefs[ann->nxrefs].text, sizeof(ann->xrefs[ann->nxrefs].text), val);
                        last_text = ann->xrefs[ann->nxrefs].text;
                        last_text_sz = sizeof(ann->xrefs[ann->nxrefs].text);
                        ann->nxrefs++;
                    }
                }
            }
        } else if (last_text) {
            /* Continuation line */
            char *cont = line;
            if (isspace(line[0])) {
                while (*cont && isspace(*cont)) cont++;
            }
            unescape_newlines(cont);
            size_t curlen = strlen(last_text);
            if (curlen + 1 < last_text_sz) {
                strcat(last_text, "\n");
                curlen++;
                strncat(last_text, cont, last_text_sz - curlen - 1);
            }
        }
    }
    if (last_text) trim_trailing_newlines(last_text);

    fclose(fp);
    return ann;
}

static void save_text_field(FILE *fp, const char *key, const char *text) {
    fprintf(fp, "%s = ", key);
    for (const char *p = text; *p; p++) {
        if (*p == '\n') {
            fprintf(fp, "\n              "); /* Indent to match "text = " (approx) */
        } else {
            fputc(*p, fp);
        }
    }
    fprintf(fp, "\n");
}

int annotate_save(const AnnData *ann, const char *path) {
    if (!ann) return -1;
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;

    fprintf(fp, "# z80bench annotation file\n\n");
    fprintf(fp, "[meta]\nname = %s\nload_addr = 0x%04X\n\n",
            ann->name, ann->load_addr);

    for (int i = 0; i < ann->nregions; i++) {
        fprintf(fp, "[region]\noffset = %d\nend = %d\ntype = ", ann->regions[i].offset, ann->regions[i].end);
        switch (ann->regions[i].type) {
            case RTYPE_CODE: fprintf(fp, "CODE\n"); break;
            case RTYPE_DATA_BYTE: fprintf(fp, "DATA_BYTE\n"); break;
            case RTYPE_DATA_WORD: fprintf(fp, "DATA_WORD\n"); break;
            case RTYPE_DATA_STRING: fprintf(fp, "DATA_STRING\n"); break;
            case RTYPE_GAP: fprintf(fp, "GAP\n"); break;
            default: fprintf(fp, "GAP\n"); break;
        }
        fprintf(fp, "\n");
    }

    for (int i = 0; i < ann->nlabels; i++) {
        fprintf(fp, "[label]\noffset = %d\nname = %s\n\n", ann->labels[i].offset, ann->labels[i].name);
    }
    for (int i = 0; i < ann->ncomments; i++) {
        fprintf(fp, "[comment]\noffset = %d\n", ann->comments[i].offset);
        save_text_field(fp, "text", ann->comments[i].text);
        fprintf(fp, "\n");
    }
    for (int i = 0; i < ann->nblocks; i++) {
        fprintf(fp, "[block]\noffset = %d\n", ann->blocks[i].offset);
        save_text_field(fp, "text", ann->blocks[i].text);
        fprintf(fp, "\n");
    }
    for (int i = 0; i < ann->nxrefs; i++) {
        fprintf(fp, "[xref]\noffset = %d\n", ann->xrefs[i].offset);
        save_text_field(fp, "text", ann->xrefs[i].text);
        fprintf(fp, "\n");
    }

    fclose(fp);
    return 0;
}

void annotate_free(AnnData *ann) {
    free(ann);
}

void annotate_get_meta_safe(const AnnData *ann, char *name, size_t name_sz,
                            int *load_addr) {
    if (!ann) return;
    if (name && name_sz > 0) {
        snprintf(name, name_sz, "%s", ann->name);
    }
    if (load_addr) *load_addr = ann->load_addr;
}

void annotate_get_meta(const AnnData *ann, char *name, int *load_addr) {
    annotate_get_meta_safe(ann, name, name ? DISASM_LABEL_MAX : 0, load_addr);
}

const Region *annotate_get_regions(const AnnData *ann, int *nregions) {
    if (!ann) return NULL;
    if (nregions) *nregions = ann->nregions;
    return ann->regions;
}

void annotate_merge(DisasmLine *lines, int nlines, const AnnData *ann) {
    if (!ann) return;
    for (int i = 0; i < nlines; i++) {
        DisasmLine *dl = &lines[i];
        for (int j = 0; j < ann->nlabels; j++) {
            if (ann->labels[j].offset == dl->offset) { copy_text(dl->label, sizeof(dl->label), ann->labels[j].name); break; }
        }
        for (int j = 0; j < ann->ncomments; j++) {
            if (ann->comments[j].offset == dl->offset) { copy_text(dl->comment, sizeof(dl->comment), ann->comments[j].text); break; }
        }
        for (int j = 0; j < ann->nblocks; j++) {
            if (ann->blocks[j].offset == dl->offset) { copy_text(dl->block, sizeof(dl->block), ann->blocks[j].text); break; }
        }
        for (int j = 0; j < ann->nxrefs; j++) {
            if (ann->xrefs[j].offset == dl->offset) { copy_text(dl->xref, sizeof(dl->xref), ann->xrefs[j].text); break; }
        }
    }
}

int xref_build(const DisasmLine *lines, int nlines, XrefEntry *out, int out_max) {
    int nentries = 0;
    for (int i = 0; i < nlines; i++) {
        const DisasmLine *dl = &lines[i];
        if (dl->rtype == RTYPE_ORPHAN) {
            if (nentries < out_max) {
                XrefEntry *xe = &out[nentries++];
                memset(xe, 0, sizeof(XrefEntry));
                xe->addr = dl->addr; xe->xtype = XREF_ORPHAN; xe->in_rom = 1;
                xe->first_byte_count = dl->byte_count > 3 ? 3 : dl->byte_count;
                memcpy(xe->first_bytes, dl->bytes, xe->first_byte_count);
            }
        }
        if (dl->operand_addr != -1) {
            int found = 0;
            for (int j = 0; j < nentries; j++) {
                if (out[j].xtype == XREF_ADDR && out[j].addr == dl->operand_addr) { out[j].ref_count++; found = 1; break; }
            }
            if (!found && nentries < out_max) {
                XrefEntry *xe = &out[nentries++];
                memset(xe, 0, sizeof(XrefEntry));
                xe->addr = dl->operand_addr; xe->xtype = XREF_ADDR; xe->ref_count = 1;
            }
        }
    }
    return nentries;
}

/*
 * annotate_sync_from_lines()
 *
 * Rebuild the label/comment/block/xref arrays in AnnData from the live
 * DisasmLine data.  Call this before annotate_save() so edits made through
 * the UI are persisted.  Regions and meta are left untouched.
 */
void annotate_sync_from_lines(AnnData *ann, const DisasmLine *lines, int nlines) {
    if (!ann) return;

    ann->nlabels   = 0;
    ann->ncomments = 0;
    ann->nblocks   = 0;
    ann->nxrefs    = 0;

    for (int i = 0; i < nlines; i++) {
        const DisasmLine *dl = &lines[i];

        if (dl->label[0] && ann->nlabels < MAX_ANNOTATIONS) {
            ann->labels[ann->nlabels].offset = dl->offset;
            copy_text(ann->labels[ann->nlabels].name, sizeof(ann->labels[ann->nlabels].name), dl->label);
            ann->nlabels++;
        }
        if (dl->comment[0] && ann->ncomments < MAX_ANNOTATIONS) {
            ann->comments[ann->ncomments].offset = dl->offset;
            copy_text(ann->comments[ann->ncomments].text, sizeof(ann->comments[ann->ncomments].text), dl->comment);
            ann->ncomments++;
        }
        if (dl->block[0] && ann->nblocks < MAX_ANNOTATIONS) {
            ann->blocks[ann->nblocks].offset = dl->offset;
            copy_text(ann->blocks[ann->nblocks].text, sizeof(ann->blocks[ann->nblocks].text), dl->block);
            ann->nblocks++;
        }
        if (dl->xref[0] && ann->nxrefs < MAX_ANNOTATIONS) {
            ann->xrefs[ann->nxrefs].offset = dl->offset;
            copy_text(ann->xrefs[ann->nxrefs].text, sizeof(ann->xrefs[ann->nxrefs].text), dl->xref);
            ann->nxrefs++;
        }
    }
}

AnnData *annotate_new(const char *name, int load_addr) {
    AnnData *ann = calloc(1, sizeof(AnnData));
    if (!ann) return NULL;
    if (name) copy_text(ann->name, sizeof(ann->name), name);
    ann->load_addr  = load_addr;
    return ann;
}

void annotate_set_regions(AnnData *ann, const Region *regions, int nregions) {
    if (!ann || !regions || nregions <= 0) {
        if (ann) ann->nregions = 0;
        return;
    }
    int n = nregions < MAX_REGIONS ? nregions : MAX_REGIONS;
    memcpy(ann->regions, regions, n * sizeof(Region));
    ann->nregions = n;
}

int annotate_get_region_safe(const AnnData *ann, int i, Region *out) {
    if (!ann || !out || i < 0 || i >= ann->nregions) return -1;
    *out = ann->regions[i];
    return 0;
}
