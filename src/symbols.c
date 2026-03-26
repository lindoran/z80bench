#include "z80bench_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SYMBOLS 1000

struct SymData {
    Symbol symbols[MAX_SYMBOLS];
    int nsymbols;
};

static void copy_text(char *dst, size_t dst_sz, const char *src) {
    if (!dst || dst_sz == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, dst_sz, "%s", src);
}

SymData *symbols_load(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return NULL;

    SymData *sym = calloc(1, sizeof(SymData));
    if (!sym) {
        fclose(fp);
        return NULL;
    }

    char line[512];
    SymbolType current_type = SYM_CONSTANT;

    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == ';' && strchr(line, '[')) {
            if (strstr(line, "[ROM_CALL]")) current_type = SYM_ROM_CALL;
            else if (strstr(line, "[VECTOR]")) current_type = SYM_VECTOR;
            else if (strstr(line, "[JUMP_LABEL]")) current_type = SYM_JUMP_LABEL;
            else if (strstr(line, "[WRITABLE]")) current_type = SYM_WRITABLE;
            else if (strstr(line, "[PORT]")) current_type = SYM_PORT;
            else if (strstr(line, "[CONSTANT]")) current_type = SYM_CONSTANT;
            continue;
        }

        if (line[0] == ';' || line[0] == '\n' || line[0] == '\r') continue;

        char name[DISASM_LABEL_MAX];
        char addr_str[64];
        if (sscanf(line, "%63s EQU %63s", name, addr_str) == 2) {
            if (sym->nsymbols < MAX_SYMBOLS) {
                Symbol *s = &sym->symbols[sym->nsymbols++];
                copy_text(s->name, sizeof(s->name), name);
                if (strncmp(addr_str, "0x", 2) == 0 || strncmp(addr_str, "0X", 2) == 0) {
                    s->addr = (int)strtol(addr_str + 2, NULL, 16);
                } else {
                    char *end;
                    s->addr = (int)strtol(addr_str, &end, 16);
                }
                s->type = current_type;
                char *comment = strchr(line, ';');
                if (comment) {
                    copy_text(s->notes, sizeof(s->notes), comment + 1);
                    char *nl = strchr(s->notes, '\n');
                    if (nl) *nl = '\0';
                }
            }
        }
    }
    fclose(fp);
    return sym;
}

int symbols_save(const SymData *sym, const char *path) {
    if (!sym) return -1;
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;
    fprintf(fp, "; z80bench symbol file\n");
    fprintf(fp, "; assembler: z88dk/z80asm\n\n");
    SymbolType last_type = (SymbolType)-1;
    for (int i = 0; i < sym->nsymbols; i++) {
        if (sym->symbols[i].type != last_type) {
            const char *tag = "CONSTANT";
            switch (sym->symbols[i].type) {
                case SYM_ROM_CALL: tag = "ROM_CALL"; break;
                case SYM_VECTOR: tag = "VECTOR"; break;
                case SYM_JUMP_LABEL: tag = "JUMP_LABEL"; break;
                case SYM_WRITABLE: tag = "WRITABLE"; break;
                case SYM_PORT: tag = "PORT"; break;
                case SYM_CONSTANT: tag = "CONSTANT"; break;
            }
            fprintf(fp, "\n; [%s]\n", tag);
            last_type = sym->symbols[i].type;
        }
        fprintf(fp, "%-16s EQU     0x%04X      ; %s\n", sym->symbols[i].name, sym->symbols[i].addr, sym->symbols[i].notes);
    }
    fclose(fp);
    return 0;
}

void symbols_free(SymData *sym) {
    free(sym);
}

void symbols_resolve(DisasmLine *lines, int nlines, const SymData *sym) {
    if (!sym) return;
    for (int i = 0; i < nlines; i++) {
        DisasmLine *dl = &lines[i];
        if (dl->operand_addr != -1) {
            for (int j = 0; j < sym->nsymbols; j++) {
                if (sym->symbols[j].addr == dl->operand_addr) {
                    copy_text(dl->sym_name, sizeof(dl->sym_name), sym->symbols[j].name);
                    dl->sym_type = sym->symbols[j].type;
                    break;
                }
            }
        }
    }
}

SymData *symbols_new(void) { return calloc(1, sizeof(SymData)); }

int symbols_count(const SymData *sym) { return sym ? sym->nsymbols : 0; }

const Symbol *symbols_get(const SymData *sym, int i) {
    if (!sym || i < 0 || i >= sym->nsymbols) return NULL;
    return &sym->symbols[i];
}

int symbols_add(SymData *sym, const Symbol *s) {
    if (!sym || !s || sym->nsymbols >= MAX_SYMBOLS) return -1;
    sym->symbols[sym->nsymbols++] = *s;
    return 0;
}

int symbols_remove(SymData *sym, int i) {
    if (!sym || i < 0 || i >= sym->nsymbols) return -1;
    memmove(&sym->symbols[i], &sym->symbols[i + 1],
            (sym->nsymbols - i - 1) * sizeof(Symbol));
    sym->nsymbols--;
    return 0;
}

int symbols_replace(SymData *sym, int i, const Symbol *s) {
    if (!sym || !s || i < 0 || i >= sym->nsymbols) return -1;
    sym->symbols[i] = *s;
    return 0;
}

int symbols_get_safe(const SymData *sym, int i, Symbol *out) {
    if (!sym || !out || i < 0 || i >= sym->nsymbols) return -1;
    *out = sym->symbols[i];
    return 0;
}
