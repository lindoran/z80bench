#include "z80bench_core.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/*
 * export.c — .lst and .asm export matching the z80bench listing layout.
 *
 * Column layout (must match ui_listing.c constants):
 *
 *   Listing zone:  addr(6) + bytes(9) + ascii(6) = 21 chars
 *   Assembly zone: starts at col 21
 *     COL_LABEL = 21  — labels and block comment ; lines
 *     COL_MNEM  = 35  — mnemonics (14 chars into assembly zone)
 *     COL_OPS   = 41  — operands (6 chars after mnemonic)
 *     COL_CMT   = 56  — inline comments
 *     COL_MAX   = 79
 */

#define EXP_COL_LABEL   21
#define EXP_COL_MNEM    35
#define EXP_COL_OPS     41
#define EXP_COL_CMT     56
#define EXP_COL_MAX     100
#define EXP_BLOCK_AVAIL (EXP_COL_MAX - EXP_COL_LABEL - 2)  /* chars after "; " */
#define EXP_CMT_MAX     (EXP_COL_MAX - EXP_COL_CMT - 2)

/*
 * Write block comment text to fp, one "; line\n" per logical/wrapped line.
 * indent = number of spaces before the "; ".
 */
static void write_block(FILE *fp, const char *text, int indent) {
    const char *p = text;
    /* Wrap at 77 chars (79 - 2) so block comments are consistent regardless of indent.
     * In .asm this means total width is 79. In .lst (indent 21) it means width is 100. */
    int avail = 77;

    while (*p) {
        const char *nl = strchr(p, '\n');
        int line_len = nl ? (int)(nl - p) : (int)strlen(p);

        if (line_len == 0) {
            fprintf(fp, "%*s;\n", indent, "");
            p++;
            continue;
        }

        int consumed = 0;
        while (consumed < line_len) {
            int remaining = line_len - consumed;
            int chunk = remaining <= avail ? remaining : avail;

            if (chunk < remaining && p[consumed + chunk] != ' ') {
                int j = chunk;
                while (j > 0 && p[consumed + j - 1] != ' ') j--;
                if (j > 0) chunk = j;
            }

            fprintf(fp, "%*s; %.*s\n", indent, "", chunk, p + consumed);
            consumed += chunk;
            while (consumed < line_len && p[consumed] == ' ') consumed++;
        }

        p += line_len;
        if (*p == '\n') p++;
    }
}

/* ============================================================
 * .lst export — matches the window listing layout exactly
 * ============================================================ */

int export_lst(const Project *p, const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;

    fprintf(fp, "; z80bench listing — %s\n", p->name);
    fprintf(fp, ";\n");
    fprintf(fp, "; %-6s %-9s %-6s %-*s%-5s %-*s%s\n",
            "ADDR", "BYTES", "ASCII",
            EXP_COL_MNEM - EXP_COL_LABEL, "LABEL",
            "MNEM",
            EXP_COL_CMT  - EXP_COL_OPS - 6, "OPERANDS",
            "COMMENT");
    fprintf(fp, ";\n\n");

    for (int i = 0; i < p->nlines; i++) {
        const DisasmLine *dl = &p->lines[i];

        /* Block comment — at COL_LABEL, "; " prefix every line */
        if (dl->block[0])
            write_block(fp, dl->block, EXP_COL_LABEL);

        /* Label — at COL_LABEL */
        if (dl->label[0])
            fprintf(fp, "%*s%s:\n", EXP_COL_LABEL, "", dl->label);

        /* Listing zone: addr + compact bytes + ascii */
        const unsigned char *rom_ptr =
            (p->rom && dl->offset >= 0 && dl->offset < p->rom_size)
            ? (p->rom + dl->offset) : dl->bytes;

        int total = dl->byte_count;
        int rows  = (total + 3) / 4;
        if (rows < 1) rows = 1;

        for (int row = 0; row < rows; row++) {
            int base       = row * 4;
            int row_count  = total - base;
            if (row_count > 4) row_count = 4;

            /* addr (first row only) */
            if (row == 0)
                fprintf(fp, "%04X  ", dl->addr);
            else
                fprintf(fp, "      ");  /* 6 spaces */

            /* bytes — compact, padded to 9 */
            char bytes_str[12] = "";
            for (int b = 0; b < row_count; b++) {
                char tmp[3];
                sprintf(tmp, "%02X", rom_ptr[base + b]);
                strcat(bytes_str, tmp);
            }
            int blen = strlen(bytes_str);
            while (blen < 9) bytes_str[blen++] = ' ';
            bytes_str[9] = '\0';
            fprintf(fp, "%s", bytes_str);

            /* ascii — 4 chars + 2 spaces = 6 */
            char ascii_str[7] = "      ";
            for (int b = 0; b < row_count; b++) {
                unsigned char c = rom_ptr[base + b];
                ascii_str[b] = (c >= 0x20 && c < 0x7F) ? (char)c : '.';
            }
            ascii_str[4] = ' '; ascii_str[5] = ' '; ascii_str[6] = '\0';
            fprintf(fp, "%s", ascii_str);

            /* assembly zone — only on first row */
            if (row == 0) {
                /* pad from col 21 to COL_MNEM */
                fprintf(fp, "%*s", EXP_COL_MNEM - EXP_COL_LABEL, "");

                /* mnemonic — 5 chars padded */
                fprintf(fp, "%-5s ", dl->mnemonic);

                /* operands */
                int ops_len = strlen(dl->operands);
                fprintf(fp, "%s", dl->operands);

                /* inline comment — pad to COL_CMT */
                if (dl->comment[0]) {
                    int cur_col = EXP_COL_MNEM + 6 + ops_len;
                    int pad = EXP_COL_CMT - cur_col;
                    if (pad < 1) pad = 1;
                    fprintf(fp, "%*s; %.*s", pad, "", EXP_CMT_MAX, dl->comment);
                }

                if (dl->rtype == RTYPE_ORPHAN)
                    fprintf(fp, " ; *** ORPHAN ***");
            }

            fprintf(fp, "\n");
        }
    }

    fclose(fp);
    return 0;
}

/* ============================================================
 * .asm export — assembly zone only, no listing columns
 * ============================================================ */

/* ASM column constants (assembly-only file) */
#define ASM_COL_MNEM   14   /* matches (COL_MNEM - COL_LABEL) = 35 - 21 */
#define ASM_COL_OPS    20   /* matches (COL_OPS - COL_LABEL) = 41 - 21 */
#define ASM_COL_CMT    35   /* matches (COL_CMT - COL_LABEL) = 56 - 21 */
#define ASM_CMT_MAX    (EXP_COL_MAX - ASM_COL_CMT - 2)

int export_asm(const Project *p, const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;

    time_t now = time(NULL);
    fprintf(fp, "; z80bench export — %s\n", p->name);
    fprintf(fp, "; Generated: %s", ctime(&now));
    fprintf(fp, "; Assembler: z88dk/z80asm\n\n");

    fprintf(fp, "        INCLUDE \"%s\"\n", Z80BENCH_SYM_FILE);
    fprintf(fp, "        ORG     0x%04X\n\n", p->load_addr);

    for (int i = 0; i < p->nlines; i++) {
        const DisasmLine *dl = &p->lines[i];

        /* Block comment — no indent (col 0 of the asm file), "; " every line */
        if (dl->block[0]) {
            fprintf(fp, "\n");
            write_block(fp, dl->block, 0);
        }

        /* Label at col 0 */
        if (dl->label[0])
            fprintf(fp, "%s:\n", dl->label);

        /* Mnemonic + operands indented */
        int ops_len = strlen(dl->operands);
        fprintf(fp, "%-*s%-5s ", ASM_COL_MNEM, "", dl->mnemonic);
        fprintf(fp, "%s", dl->operands);

        /* Inline comment — pad to ASM_COL_CMT */
        if (dl->comment[0]) {
            int cur_col = ASM_COL_MNEM + 6 + ops_len;
            int pad = ASM_COL_CMT - cur_col;
            if (pad < 1) pad = 1;
            fprintf(fp, "%*s; %.*s", pad, "", ASM_CMT_MAX, dl->comment);
        }

        fprintf(fp, "\n");
    }

    fclose(fp);
    return 0;
}
