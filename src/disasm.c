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

static int is_ident_char(char c) {
    return (c == '_') || isalnum((unsigned char)c);
}

static void format_hex_literal(char *out, size_t out_sz, unsigned long value,
                               int digits_hint, int prefer_byte_width) {
    int width;
    if (prefer_byte_width && value <= 0xFFUL) {
        width = 2;
    } else {
        width = (digits_hint <= 2 && value <= 0xFFUL) ? 2 : 4;
    }
    snprintf(out, out_sz, "0x%0*lX", width, value & 0xFFFFUL);
}

/* Normalize hex literal styles in operands to a single format:
 *   $12, 12h, 0012h, 0x12  ->  0x12 / 0x0012 (2 or 4 digits)
 * This makes listing/.lst/.asm render consistently because they all share dl->operands.
 */
static void normalize_operand_hex(char *operands, size_t operands_sz, int prefer_byte_width) {
    if (!operands || operands_sz == 0 || !operands[0]) return;

    char in[DISASM_OPERANDS_MAX];
    strncpy(in, operands, sizeof(in) - 1);
    in[sizeof(in) - 1] = '\0';

    char out[DISASM_OPERANDS_MAX];
    size_t oi = 0;
    size_t n = strlen(in);

    for (size_t i = 0; i < n && oi + 1 < sizeof(out); ) {
        char prev = (i > 0) ? in[i - 1] : '\0';
        int start_ok = (i == 0) || !is_ident_char(prev);
        int matched = 0;

        if (start_ok && in[i] == '$' && i + 1 < n && isxdigit((unsigned char)in[i + 1])) {
            size_t j = i + 1;
            while (j < n && isxdigit((unsigned char)in[j])) j++;
            char next = (j < n) ? in[j] : '\0';
            if (!is_ident_char(next)) {
                char hex[17];
                size_t dlen = j - (i + 1);
                if (dlen >= sizeof(hex)) dlen = sizeof(hex) - 1;
                memcpy(hex, in + i + 1, dlen);
                hex[dlen] = '\0';
                unsigned long v = strtoul(hex, NULL, 16);
                char lit[16];
                format_hex_literal(lit, sizeof(lit), v, (int)dlen, prefer_byte_width);
                size_t llen = strlen(lit);
                if (oi + llen < sizeof(out)) {
                    memcpy(out + oi, lit, llen);
                    oi += llen;
                    i = j;
                    matched = 1;
                }
            }
        }

        if (!matched && start_ok &&
            in[i] == '0' && i + 2 < n &&
            (in[i + 1] == 'x' || in[i + 1] == 'X') &&
            isxdigit((unsigned char)in[i + 2])) {
            size_t j = i + 2;
            while (j < n && isxdigit((unsigned char)in[j])) j++;
            char next = (j < n) ? in[j] : '\0';
            if (!is_ident_char(next)) {
                char hex[17];
                size_t dlen = j - (i + 2);
                if (dlen >= sizeof(hex)) dlen = sizeof(hex) - 1;
                memcpy(hex, in + i + 2, dlen);
                hex[dlen] = '\0';
                unsigned long v = strtoul(hex, NULL, 16);
                char lit[16];
                format_hex_literal(lit, sizeof(lit), v, (int)dlen, prefer_byte_width);
                size_t llen = strlen(lit);
                if (oi + llen < sizeof(out)) {
                    memcpy(out + oi, lit, llen);
                    oi += llen;
                    i = j;
                    matched = 1;
                }
            }
        }

        if (!matched && start_ok && isxdigit((unsigned char)in[i])) {
            size_t j = i;
            while (j < n && isxdigit((unsigned char)in[j])) j++;
            if (j < n && (in[j] == 'h' || in[j] == 'H')) {
                char next = (j + 1 < n) ? in[j + 1] : '\0';
                if (!is_ident_char(next)) {
                    char hex[17];
                    size_t dlen = j - i;
                    if (dlen >= sizeof(hex)) dlen = sizeof(hex) - 1;
                    memcpy(hex, in + i, dlen);
                    hex[dlen] = '\0';
                    unsigned long v = strtoul(hex, NULL, 16);
                    char lit[16];
                    format_hex_literal(lit, sizeof(lit), v, (int)dlen, prefer_byte_width);
                    size_t llen = strlen(lit);
                    if (oi + llen < sizeof(out)) {
                        memcpy(out + oi, lit, llen);
                        oi += llen;
                        i = j + 1; /* consume trailing h/H */
                        matched = 1;
                    }
                }
            }
        }

        if (!matched) {
            out[oi++] = in[i++];
        }
    }

    out[oi] = '\0';
    if (operands_sz > 0) {
        strncpy(operands, out, operands_sz - 1);
        operands[operands_sz - 1] = '\0';
    }
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
                normalize_operand_hex(dl->operands, sizeof(dl->operands),
                                      opctx_prefers_byte_hex_width(dl));

                if (is_orphan(dl->mnemonic, dl->operands)) {
                    dl->rtype = RTYPE_ORPHAN;
                    dl->byte_count = 1;
                    strcpy(dl->mnemonic, "DEFB");
                    snprintf(dl->operands, sizeof(dl->operands),
                             "0x%02X", rom[offset_from_comment]);
                } else {
                    dl->operand_addr = opctx_compute_operand_value(dl);
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
                            snprintf(tmp, sizeof(tmp), b ? ",0x%02X" : "0x%02X",
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
            normalize_operand_hex(dl->operands, sizeof(dl->operands),
                                  opctx_prefers_byte_hex_width(dl));
            
            dl->operand_addr = opctx_compute_operand_value(dl);
            nlines++;
        }
    }

    fclose(fp);
    return nlines;
}
