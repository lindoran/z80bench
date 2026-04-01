#include "z80bench_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_SYMBOLS 1000

struct SymData {
    Symbol symbols[MAX_SYMBOLS];
    int nsymbols;
};

typedef struct {
    int         addr;
    const char *name;
} AddrName;

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

static int addr_name_cmp(const void *a, const void *b) {
    const AddrName *aa = (const AddrName *)a;
    const AddrName *bb = (const AddrName *)b;
    if (aa->addr < bb->addr) return -1;
    if (aa->addr > bb->addr) return 1;
    return 0;
}

static const char *lookup_addr_name(const AddrName *arr, int n, int addr) {
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = lo + ((hi - lo) / 2);
        if (arr[mid].addr == addr) {
            while (mid > 0 && arr[mid - 1].addr == addr) mid--;
            return arr[mid].name;
        }
        if (arr[mid].addr < addr) lo = mid + 1;
        else hi = mid - 1;
    }
    return NULL;
}

static int is_ident_char(char c) {
    return (c == '_') || isalnum((unsigned char)c);
}

static int find_matching_hex_literal(const char *s, int addr, int *out_start, int *out_end) {
    if (!s) return 0;
    unsigned int target = (unsigned int)addr & 0xFFFFU;
    size_t n = strlen(s);

    for (size_t i = 0; i < n; i++) {
        char prev = (i > 0) ? s[i - 1] : '\0';
        int start_ok = (i == 0) || !is_ident_char(prev);

        if (start_ok &&
            s[i] == '0' && i + 2 < n &&
            (s[i + 1] == 'x' || s[i + 1] == 'X') &&
            isxdigit((unsigned char)s[i + 2])) {
            size_t j = i + 2;
            while (j < n && isxdigit((unsigned char)s[j])) j++;
            char next = (j < n) ? s[j] : '\0';
            if (!is_ident_char(next)) {
                char hex[17];
                size_t len = j - (i + 2);
                if (len >= sizeof(hex)) len = sizeof(hex) - 1;
                memcpy(hex, s + i + 2, len);
                hex[len] = '\0';
                unsigned long v = strtoul(hex, NULL, 16) & 0xFFFFUL;
                if ((unsigned int)v == target) {
                    *out_start = (int)i;
                    *out_end = (int)j;
                    return 1;
                }
            }
            continue;
        }

        if (start_ok && s[i] == '$' && i + 1 < n && isxdigit((unsigned char)s[i + 1])) {
            size_t j = i + 1;
            while (j < n && isxdigit((unsigned char)s[j])) j++;
            char next = (j < n) ? s[j] : '\0';
            if (!is_ident_char(next)) {
                char hex[17];
                size_t len = j - (i + 1);
                if (len >= sizeof(hex)) len = sizeof(hex) - 1;
                memcpy(hex, s + i + 1, len);
                hex[len] = '\0';
                unsigned long v = strtoul(hex, NULL, 16) & 0xFFFFUL;
                if ((unsigned int)v == target) {
                    *out_start = (int)i;
                    *out_end = (int)j;
                    return 1;
                }
            }
            continue;
        }

        if (start_ok && isxdigit((unsigned char)s[i])) {
            size_t j = i;
            while (j < n && isxdigit((unsigned char)s[j])) j++;
            if (j < n && (s[j] == 'h' || s[j] == 'H')) {
                char next = (j + 1 < n) ? s[j + 1] : '\0';
                if (!is_ident_char(next)) {
                    char hex[17];
                    size_t len = j - i;
                    if (len >= sizeof(hex)) len = sizeof(hex) - 1;
                    memcpy(hex, s + i, len);
                    hex[len] = '\0';
                    unsigned long v = strtoul(hex, NULL, 16) & 0xFFFFUL;
                    if ((unsigned int)v == target) {
                        *out_start = (int)i;
                        *out_end = (int)(j + 1); /* include trailing h/H */
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

static int format_operands_with_symbol(const DisasmLine *dl, const char *name,
                                       char *out, size_t out_sz) {
    if (!dl || !name || !name[0] || !out || out_sz == 0 || !dl->operands[0] || dl->operand_addr < 0)
        return 0;

    copy_text(out, out_sz, dl->operands);
    int start = 0, end = 0;
    if (!find_matching_hex_literal(out, dl->operand_addr, &start, &end))
        return 0;

    size_t prefix = (size_t)start;
    size_t suffix = strlen(out + end);
    size_t nlen = strlen(name);
    if (prefix + nlen + suffix >= out_sz)
        return 0;

    char tmp[DISASM_OPERANDS_MAX];
    memcpy(tmp, out, prefix);
    memcpy(tmp + prefix, name, nlen);
    memcpy(tmp + prefix + nlen, out + end, suffix);
    tmp[prefix + nlen + suffix] = '\0';
    copy_text(out, out_sz, tmp);
    return 1;
}

void symbols_resolve(DisasmLine *lines, int nlines, const SymData *sym) {
    if (!lines || nlines <= 0) return;

    AddrName *labels = calloc((size_t)nlines, sizeof(AddrName));
    int nlabels = 0;
    if (labels) {
        for (int i = 0; i < nlines; i++) {
            if (lines[i].label[0]) {
                labels[nlabels].addr = lines[i].addr;
                labels[nlabels].name = lines[i].label;
                nlabels++;
            }
        }
        if (nlabels > 1)
            qsort(labels, (size_t)nlabels, sizeof(AddrName), addr_name_cmp);
    }

    for (int i = 0; i < nlines; i++) {
        DisasmLine *dl = &lines[i];
        dl->sym_name[0] = '\0';
        dl->sym_type = -1;

        if (dl->operand_addr != -1) {
            int allow_abs = opctx_has_absolute_operand_ref(dl);
            int allow_const = opctx_has_constant_immediate_operand(dl);
            if (!allow_abs && !allow_const)
                continue;

            const char *resolved = NULL;
            if (sym) {
                for (int j = 0; j < sym->nsymbols; j++) {
                    if (sym->symbols[j].addr == dl->operand_addr) {
                        if (!allow_abs && sym->symbols[j].type != SYM_CONSTANT)
                            continue;
                        resolved = sym->symbols[j].name;
                        dl->sym_type = sym->symbols[j].type;
                        break;
                    }
                }
            }

            if (!resolved && allow_abs && labels && nlabels > 0)
                resolved = lookup_addr_name(labels, nlabels, dl->operand_addr);

            if (resolved && resolved[0]) {
                copy_text(dl->sym_name, sizeof(dl->sym_name), resolved);
            }
        }
    }

    free(labels);
}

void symbols_format_operands(const DisasmLine *dl, char *out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    out[0] = '\0';
    if (!dl) return;

    copy_text(out, out_sz, dl->operands);
    if (!dl->sym_name[0] || dl->operand_addr < 0)
        return;

    int allow_abs = opctx_has_absolute_operand_ref(dl);
    int allow_const = opctx_has_constant_immediate_operand(dl);
    if (!allow_abs && !allow_const)
        return;
    if (!allow_abs && dl->sym_type != SYM_CONSTANT)
        return;

    (void)format_operands_with_symbol(dl, dl->sym_name, out, out_sz);
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
