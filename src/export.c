#include "z80bench_core.h"
#include <stdio.h>
#include <time.h>

int export_lst(const Project *p, const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;

    fprintf(fp, "; z80bench export — %s\n", p->name);
    fprintf(fp, "; format: addr bytes mnemonic operands ; comment\n\n");

    for (int i = 0; i < p->nlines; i++) {
        DisasmLine *dl = &p->lines[i];

        if (dl->block[0]) {
            fprintf(fp, "; %s\n", dl->block);
        }
        if (dl->label[0]) {
            fprintf(fp, "; ---- %s ------------------------------------------------\n", dl->label);
        }

        fprintf(fp, "%04X  ", dl->addr);
        for (int b = 0; b < 4; b++) {
            if (b < dl->byte_count) fprintf(fp, "%02X ", dl->bytes[b]);
            else fprintf(fp, "   ");
        }
        
        fprintf(fp, "   %-5s %-20s", dl->mnemonic, dl->operands);
        
        if (dl->comment[0]) {
            fprintf(fp, " ; %s", dl->comment);
        }
        if (dl->rtype == RTYPE_ORPHAN) {
            fprintf(fp, " ; *** ORPHAN ***");
        }
        fprintf(fp, "\n");
    }

    fclose(fp);
    return 0;
}

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
        DisasmLine *dl = &p->lines[i];

        if (dl->block[0]) {
            fprintf(fp, "\n; %s\n", dl->block);
        }
        if (dl->label[0]) {
            fprintf(fp, "%s:\n", dl->label);
        }

        fprintf(fp, "        %-8s %-20s", dl->mnemonic, dl->operands);
        
        if (dl->comment[0]) {
            fprintf(fp, " ; %s", dl->comment);
        }
        fprintf(fp, "\n");
    }

    fclose(fp);
    return 0;
}
