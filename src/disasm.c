#define _DEFAULT_SOURCE
#include "z80bench_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <ctype.h>

static char *shell_quote(const char *s) {
    size_t extra = 2; /* outer quotes */
    for (const char *p = s; *p; p++)
        extra += (*p == '\'') ? 4 : 1;

    char *out = malloc(extra + 1);
    if (!out) return NULL;

    char *d = out;
    *d++ = '\'';
    for (const char *p = s; *p; p++) {
        if (*p == '\'') {
            memcpy(d, "'\\''", 4);
            d += 4;
        } else {
            *d++ = *p;
        }
    }
    *d++ = '\'';
    *d = '\0';
    return out;
}

/* Standard Z80 instruction sizes (1, 2, or 3 bytes) */
static const unsigned char main_sizes[256] = {
    1,3,3,1,1,1,2,1,1,1,3,1,1,1,2,1, 2,3,3,1,1,1,2,1,2,1,3,1,1,1,2,1,
    2,3,3,1,1,1,2,1,2,1,3,1,1,1,2,1, 2,3,3,1,1,1,2,1,2,1,3,1,1,1,2,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,3,3,3,1,2,1,1,1,3,0,3,3,2,1, 1,1,3,2,3,1,2,1,1,1,3,2,3,0,2,1,
    1,1,3,2,3,1,2,1,1,1,3,2,3,0,2,1, 1,1,3,1,3,1,2,1,1,1,3,1,3,0,2,1
};

/* ED-prefix instruction sizes (mostly 2 bytes, some 4 for 16-bit LD) */
static const unsigned char ed_sizes[256] = {
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,4,4,2,2,2,2,2,2,4,4,2,2,2,2, 2,2,4,4,2,2,2,2,2,2,4,4,2,2,2,2,
    2,2,4,4,2,2,2,2,2,2,4,4,2,2,2,2, 2,2,4,4,2,2,2,2,2,2,4,4,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2
};

/* IX/IY-prefix instruction sizes (mostly 2 or 3 bytes, some 4) */
static const unsigned char ix_sizes[256] = {
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,4,4,2,2,2,3,2,2,2,4,2,2,2,3,2, 2,2,2,2,2,2,3,2,2,2,2,2,2,2,3,2,
    2,2,2,2,3,3,3,2,2,2,2,2,3,3,3,2, 2,2,2,2,3,3,3,2,2,2,2,2,3,3,3,2,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3, 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    2,2,2,2,3,3,3,2,2,2,2,2,3,3,3,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,3,3,3,2,2,2,2,2,3,3,3,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,3,3,3,2,2,2,2,2,3,3,3,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,3,3,3,2,2,2,2,2,3,3,3,2, 2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2
};

int disasm_instr_size(const unsigned char *rom, int offset, int rom_size) {
    if (offset < 0 || offset >= rom_size) return 0;
    
    unsigned char op = rom[offset];
    
    if (op == 0xCB) {
        return 2;
    } else if (op == 0xED) {
        if (offset + 1 >= rom_size) return 1; /* incomplete instruction */
        return ed_sizes[rom[offset + 1]];
    } else if (op == 0xDD || op == 0xFD) {
        if (offset + 1 >= rom_size) return 1; /* incomplete instruction */
        unsigned char next = rom[offset + 1];
        if (next == 0xCB) {
            return 4;
        }
        return ix_sizes[next];
    }
    
    return main_sizes[op];
}

/* Helper to parse a single line of z80dasm output.
 * z80dasm -t format:  [label:]\t<mnemonic> <operands>\t\t;<addr>\t<bytes>\t<ascii>
 * The mnemonic and operands are in ONE tab-delimited field separated only by
 * a space — there is NO tab between them. */
static int parse_z80dasm_line(const char *line, char *mnemonic, char *operands) {
    const char *p = line;

    /* Skip to after the first tab (past optional label) */
    const char *tab1 = strchr(p, '\t');
    if (!tab1) return -1;
    p = tab1 + 1;

    /* Find end of the instruction field: stops at tab (before -t comment) */
    const char *tab2 = strchr(p, '\t');
    int instr_len = tab2 ? (int)(tab2 - p) : (int)strlen(p);

    /* Trim trailing whitespace from instruction field */
    while (instr_len > 0 &&
           (p[instr_len-1] == ' ' || p[instr_len-1] == '\t' || p[instr_len-1] == '\n'))
        instr_len--;

    if (instr_len <= 0) return -1;

    /* Split on first space: everything before = mnemonic, after = operands */
    int mlen = 0;
    while (mlen < instr_len && p[mlen] != ' ') mlen++;

    if (mlen >= DISASM_MNEMONIC_MAX) mlen = DISASM_MNEMONIC_MAX - 1;
    strncpy(mnemonic, p, mlen);
    mnemonic[mlen] = '\0';

    /* Operands: skip the space, copy the rest */
    if (mlen < instr_len) {
        const char *ops = p + mlen + 1; /* skip the space */
        int olen = instr_len - mlen - 1;
        if (olen < 0) olen = 0;
        if (olen >= DISASM_OPERANDS_MAX) olen = DISASM_OPERANDS_MAX - 1;
        strncpy(operands, ops, olen);
        operands[olen] = '\0';
    } else {
        operands[0] = '\0';
    }

    return 0;
}

/* Helper to check for orphan patterns in z80dasm output */
static int is_orphan(const char *mnemonic, const char *operands) {
    if (strcasecmp(mnemonic, "defb") == 0) {
        if (strstr(operands, "invalid") || strstr(operands, "illegal") || strstr(operands, "undefined")) {
            return 1;
        }
    }
    return 0;
}

/* Helper to extract operand address */
static int extract_operand_addr(const char *operands) {
    /* Look for hex literals like 0x1234, 1234h, 01234h */
    /* This is a simplified version; in a real implementation we'd need label lookup too */
    const char *p = operands;
    while (*p) {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            return (int)strtol(p + 2, NULL, 16);
        }
        if (isxdigit(*p)) {
            char *end;
            long val = strtol(p, &end, 16);
            if (*end == 'h' || *end == 'H') return (int)val;
        }
        p++;
    }
    return -1;
}

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
) {
    if (offset_start < 0 || offset_end >= rom_size || offset_start > offset_end) {
        errno = EINVAL;
        return -1;
    }

    int nlines = 0;
    int current_offset = offset_start;

    while (current_offset <= offset_end && nlines < out_max) {
        /* Find region for current_offset */
        const Region *reg = NULL;
        for (int i = 0; i < nregions; i++) {
            if (current_offset >= regions[i].offset && current_offset <= regions[i].end) {
                reg = &regions[i];
                break;
            }
        }

        RegionType rtype = reg ? reg->type : RTYPE_CODE;
        int end_of_run;
        if (reg) {
            end_of_run = reg->end;
        } else {
            /* No region here — CODE by default, but stop before the next
             * defined region so we don't swallow data ranges. */
            end_of_run = offset_end;
            for (int i = 0; i < nregions; i++) {
                if (regions[i].offset > current_offset &&
                    regions[i].offset - 1 < end_of_run) {
                    end_of_run = regions[i].offset - 1;
                }
            }
        }
        if (end_of_run > offset_end) end_of_run = offset_end;

        if (rtype == RTYPE_CODE) {
            /* Write just this slice to a temp file — avoids needing -e */
            char slice_tmp[] = "/tmp/z80bench_slice_XXXXXX";
            int slice_fd = mkstemp(slice_tmp);
            if (slice_fd == -1) { return -1; }
            int slice_len = end_of_run - current_offset + 1;
            if (write(slice_fd, rom + current_offset, slice_len) != slice_len) {
                close(slice_fd); unlink(slice_tmp); return -1;
            }
            close(slice_fd);

            /* -t appends "; ADDR\tB1 B2 B3\tASCII" to every instruction —
             * we use those bytes directly instead of our own size tables. */
            char cmd[512];
            char *quoted_slice = shell_quote(slice_tmp);
            char *quoted_label = labelfile ? shell_quote(labelfile) : NULL;
            if (!quoted_slice || (labelfile && !quoted_label)) {
                free(quoted_slice);
                free(quoted_label);
                unlink(slice_tmp);
                return -1;
            }
            snprintf(cmd, sizeof(cmd), "z80dasm -t -g 0x%04x %s%s %s 2>/dev/null",
                     current_offset + load_addr,
                     labelfile ? "-l " : "",
                     labelfile ? quoted_label : "",
                     quoted_slice);
            free(quoted_slice);
            free(quoted_label);

            FILE *fp = popen(cmd, "r");
            if (!fp) {
                unlink(slice_tmp);
                return -1;
            }

            char line_buf[512];
            while (fgets(line_buf, sizeof(line_buf), fp) &&
                   current_offset <= end_of_run && nlines < out_max) {

                /* Skip blank lines and pure comment lines (no tab) */
                char *tab = strchr(line_buf, '\t');
                if (!tab) continue;

                /* Must have a "; ADDR ..." comment to be a real instruction */
                char *semi = strchr(line_buf, ';');
                if (!semi) continue;

                /* Parse address and raw bytes from the -t comment.
                 * Format: "; ADDR\tHH HH HH \tASCII..."
                 * Address is 4 hex digits, then a tab, then space-separated bytes. */
                char *p = semi + 1;
                while (*p == ' ' || *p == '\t') p++;

                /* Read 4-hex-digit address */
                char addr_hex[5];
                int ok = 1;
                for (int k = 0; k < 4; k++) {
                    if (!isxdigit((unsigned char)p[k])) { ok = 0; break; }
                    addr_hex[k] = p[k];
                }
                addr_hex[4] = '\0';
                if (!ok) continue;

                int addr_from_comment = (int)strtol(addr_hex, NULL, 16);
                int offset_from_comment = addr_from_comment - load_addr;

                /* Skip to the tab that follows the address */
                p += 4;
                if (*p != '\t') continue;
                p++;  /* skip the tab */

                /* Read space-separated bytes until the next tab or end */
                unsigned char raw_bytes[DISASM_BYTES_MAX];
                int raw_count = 0;
                while (raw_count < DISASM_BYTES_MAX && *p && *p != '\t') {
                    while (*p == ' ') p++;
                    if (*p == '\t' || *p == '\0') break;
                    if (!isxdigit((unsigned char)p[0]) ||
                        !isxdigit((unsigned char)p[1])) break;
                    char tmp[3] = { p[0], p[1], '\0' };
                    raw_bytes[raw_count++] = (unsigned char)strtol(tmp, NULL, 16);
                    p += 2;
                }
                if (raw_count == 0) continue;

                /* Skip if the offset is out of the ROM range */
                if (offset_from_comment < 0 || offset_from_comment >= rom_size) continue;

                DisasmLine *dl = &out[nlines];
                memset(dl, 0, sizeof(DisasmLine));
                dl->offset     = offset_from_comment;
                dl->addr       = addr_from_comment;
                dl->rtype      = RTYPE_CODE;
                dl->byte_count = raw_count;
                memcpy(dl->bytes, raw_bytes, raw_count);

                /* Extract mnemonic and operands from the tab-indented part */
                parse_z80dasm_line(line_buf, dl->mnemonic, dl->operands);

                if (is_orphan(dl->mnemonic, dl->operands)) {
                    dl->rtype = RTYPE_ORPHAN;
                    dl->byte_count = 1;
                    strcpy(dl->mnemonic, "DEFB");
                    snprintf(dl->operands, sizeof(dl->operands),
                             "0x%02X", rom[offset_from_comment]);
                } else {
                    dl->operand_addr = extract_operand_addr(dl->operands);
                }

                current_offset = offset_from_comment + dl->byte_count;
                nlines++;
            }
            pclose(fp);
            unlink(slice_tmp);
        } else {
            /* Handle DATA or GAP regions */
            int region_start = current_offset;

            if (rtype == RTYPE_DATA_STRING) {
                /* Group the entire string run into one DisasmLine */
                int run_len = end_of_run - current_offset + 1;

                DisasmLine *dl = &out[nlines];
                memset(dl, 0, sizeof(DisasmLine));
                dl->offset     = current_offset;
                dl->addr       = current_offset + load_addr;
                dl->rtype      = RTYPE_DATA_STRING;
                dl->byte_count = run_len;
                dl->operand_addr = -1;
                /* Store first 4 bytes for the bytes column */
                int store = run_len < DISASM_BYTES_MAX ? run_len : DISASM_BYTES_MAX;
                memcpy(dl->bytes, &rom[current_offset], store);
                strcpy(dl->mnemonic, "DEFM");

                /* Build operand: quoted string, non-printable as \xx */
                char *op = dl->operands;
                char *op_end = dl->operands + DISASM_OPERANDS_MAX - 2;
                *op++ = '"';
                for (int i = 0; i < run_len && op < op_end - 4; i++) {
                    unsigned char c = rom[current_offset + i];
                    if (c >= 0x20 && c < 0x7F && c != '"') {
                        *op++ = (char)c;
                    } else {
                        char esc[5];
                        snprintf(esc, sizeof(esc), "\\%02X", c);
                        if (op + 3 < op_end) {
                            memcpy(op, esc, 3); op += 3;
                        }
                    }
                }
                *op++ = '"';
                *op   = '\0';

                current_offset += run_len;
                nlines++;

            } else {
                /* DATA_BYTE, DATA_WORD, GAP — 4 bytes per DisasmLine */
                while (current_offset <= end_of_run && nlines < out_max) {
                    DisasmLine *dl = &out[nlines];
                    memset(dl, 0, sizeof(DisasmLine));
                    dl->offset = current_offset;
                    dl->addr   = current_offset + load_addr;
                    dl->rtype  = rtype;
                    dl->operand_addr = -1;

                    if (rtype == RTYPE_DATA_WORD && current_offset + 1 <= end_of_run) {
                        dl->byte_count = 2;
                        strcpy(dl->mnemonic, "DEFW");
                        unsigned int val = rom[current_offset] | (rom[current_offset+1] << 8);
                        snprintf(dl->operands, sizeof(dl->operands), "0x%04X", val);
                    } else {
                        /* Grab up to 4 bytes for this row */
                        int chunk = end_of_run - current_offset + 1;
                        if (chunk > 4) chunk = 4;
                        dl->byte_count = chunk;
                        strcpy(dl->mnemonic, "DEFB");
                        char *op = dl->operands;
                        for (int b = 0; b < chunk; b++) {
                            char tmp[8];
                            snprintf(tmp, sizeof(tmp), b ? ",$%02X" : "$%02X",
                                     rom[current_offset + b]);
                            int tlen = strlen(tmp);
                            if (op + tlen < dl->operands + DISASM_OPERANDS_MAX - 1) {
                                memcpy(op, tmp, tlen);
                                op += tlen;
                            }
                        }
                        *op = '\0';
                    }

                    memcpy(dl->bytes, &rom[current_offset], dl->byte_count);
                    current_offset += dl->byte_count;
                    nlines++;
                }
            }
            (void)region_start;
        }
    }

    return nlines;
}

int disasm_write_mnm(const DisasmLine *lines, int nlines, const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;

    fprintf(fp, "# z80bench mnemonics\n");
    fprintf(fp, "# format: offset\tbyte_count\tmnemonic\toperands\n\n");

    for (int i = 0; i < nlines; i++) {
        fprintf(fp, "%d\t%d\t%s\t%s\n",
                lines[i].offset,
                lines[i].byte_count,
                lines[i].mnemonic,
                lines[i].operands);
    }

    fclose(fp);
    return 0;
}

int disasm_read_mnm(const char          *path,
                    const unsigned char *rom,
                    int                  load_addr,
                    DisasmLine          *out,
                    int                  out_max) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    char line[512];
    int nlines = 0;

    while (fgets(line, sizeof(line), fp) && nlines < out_max) {
        if (line[0] == '#' || line[0] == '\n') continue;

        DisasmLine *dl = &out[nlines];
        memset(dl, 0, sizeof(DisasmLine));

        char mnem[DISASM_MNEMONIC_MAX];
        char ops[DISASM_OPERANDS_MAX];
        int offset, count;

        if (sscanf(line, "%d\t%d\t%15[^\t]\t%63[^\n]", &offset, &count, mnem, ops) >= 3) {
            dl->offset = offset;
            dl->addr = offset + load_addr;
            dl->byte_count = count;
            strncpy(dl->mnemonic, mnem, DISASM_MNEMONIC_MAX - 1);
            
            /* operands might be empty, so sscanf might return 3 */
            char *tab3 = strchr(line, '\t');
            if (tab3) tab3 = strchr(tab3 + 1, '\t');
            if (tab3) tab3 = strchr(tab3 + 1, '\t');
            
            if (tab3) {
                strncpy(dl->operands, tab3 + 1, DISASM_OPERANDS_MAX - 1);
                char *nl = strchr(dl->operands, '\n');
                if (nl) *nl = '\0';
            } else {
                dl->operands[0] = '\0';
            }

            if (offset >= 0 && count > 0) {
                memcpy(dl->bytes, &rom[offset], count);
            }
            
            dl->operand_addr = extract_operand_addr(dl->operands);
            nlines++;
        }
    }

    fclose(fp);
    return nlines;
}
