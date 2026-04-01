#include "z80bench_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s\n", msg); \
        return 1; \
    } \
} while (0)

static int test_opctx_core(void) {
    DisasmLine dl;
    memset(&dl, 0, sizeof(dl));
    dl.rtype = RTYPE_CODE;

    dl.byte_count = 3;
    dl.bytes[0] = 0xC3; dl.bytes[1] = 0x34; dl.bytes[2] = 0x12; /* JP 0x1234 */
    CHECK(opctx_has_absolute_operand_ref(&dl), "JP should be absolute ref");
    CHECK(!opctx_has_constant_immediate_operand(&dl), "JP should not be constant immediate");
    CHECK(!opctx_prefers_byte_hex_width(&dl), "JP should not prefer byte-width hex");
    CHECK(opctx_compute_operand_value(&dl) == 0x1234, "JP operand value should be 0x1234");

    dl.byte_count = 2;
    dl.bytes[0] = 0x06; dl.bytes[1] = 0x08; /* LD B,0x08 */
    CHECK(!opctx_has_absolute_operand_ref(&dl), "LD B,n should not be absolute ref");
    CHECK(opctx_has_constant_immediate_operand(&dl), "LD B,n should be constant immediate");
    CHECK(opctx_prefers_byte_hex_width(&dl), "LD B,n should prefer byte-width hex");
    CHECK(opctx_compute_operand_value(&dl) == 0x08, "LD B,n operand value should be 0x08");

    dl.byte_count = 2;
    dl.bytes[0] = 0x20; dl.bytes[1] = 0x2D; /* JR NZ,+0x2D */
    CHECK(!opctx_has_absolute_operand_ref(&dl), "JR NZ,e should not be absolute ref");
    CHECK(!opctx_has_constant_immediate_operand(&dl), "JR NZ,e should not be constant immediate");
    CHECK(opctx_prefers_byte_hex_width(&dl), "JR NZ,e should prefer byte-width hex");
    CHECK(opctx_compute_operand_value(&dl) == -1, "JR NZ,e should not produce symbol operand value");

    dl.byte_count = 4;
    dl.bytes[0] = 0xDD; dl.bytes[1] = 0x36; dl.bytes[2] = 0xFE; dl.bytes[3] = 0x08; /* LD (IX-2),0x08 */
    CHECK(!opctx_has_absolute_operand_ref(&dl), "LD (IX+d),n should not be absolute ref");
    CHECK(opctx_has_constant_immediate_operand(&dl), "LD (IX+d),n should be constant immediate");
    CHECK(opctx_prefers_byte_hex_width(&dl), "LD (IX+d),n should prefer byte-width hex");
    CHECK(opctx_compute_operand_value(&dl) == 0x08, "LD (IX+d),n immediate should be 0x08");

    return 0;
}

static int test_disasm_read_mnm_normalize(void) {
    const char *tmp = "/tmp/z80bench_regress.mnm";
    FILE *fp = fopen(tmp, "w");
    CHECK(fp != NULL, "fopen failed");
    fprintf(fp, "0\t2\tld\tb,08h\n");
    fprintf(fp, "2\t2\tjr\tnz,$+2Dh\n");
    fclose(fp);

    unsigned char rom[16];
    memset(rom, 0x00, sizeof(rom));
    rom[0] = 0x06; rom[1] = 0x08;
    rom[2] = 0x20; rom[3] = 0x2D;

    DisasmLine out[8];
    memset(out, 0, sizeof(out));
    int n = disasm_read_mnm(tmp, rom, 0x0000, out, 8);
    unlink(tmp);

    CHECK(n == 2, "disasm_read_mnm should return 2 lines");
    CHECK(strcmp(out[0].operands, "b,0x08") == 0, "8-bit immediate should normalize to 0x08");
    CHECK(strcmp(out[1].operands, "nz,$+0x2D") == 0, "relative displacement should normalize to 0x2D");
    return 0;
}

static int test_symbol_substitution_guards(void) {
    SymData *sym = symbols_new();
    CHECK(sym != NULL, "symbols_new failed");

    Symbol c = {0};
    snprintf(c.name, sizeof(c.name), "CONST_08");
    c.addr = 0x08;
    c.type = SYM_CONSTANT;
    CHECK(symbols_add(sym, &c) == 0, "symbols_add CONST_08 failed");

    Symbol lbl = {0};
    snprintf(lbl.name, sizeof(lbl.name), "ADDR_1234");
    lbl.addr = 0x1234;
    lbl.type = SYM_JUMP_LABEL;
    CHECK(symbols_add(sym, &lbl) == 0, "symbols_add ADDR_1234 failed");

    DisasmLine lines[4];
    memset(lines, 0, sizeof(lines));

    /* Constant-immediate context: should substitute constant symbol. */
    lines[0].rtype = RTYPE_CODE;
    lines[0].byte_count = 2;
    lines[0].bytes[0] = 0x06; lines[0].bytes[1] = 0x08;
    snprintf(lines[0].mnemonic, sizeof(lines[0].mnemonic), "ld");
    snprintf(lines[0].operands, sizeof(lines[0].operands), "b,0x08");
    lines[0].operand_addr = opctx_compute_operand_value(&lines[0]);

    /* Absolute-address context: should substitute address symbol. */
    lines[1].rtype = RTYPE_CODE;
    lines[1].byte_count = 3;
    lines[1].bytes[0] = 0xC3; lines[1].bytes[1] = 0x34; lines[1].bytes[2] = 0x12;
    snprintf(lines[1].mnemonic, sizeof(lines[1].mnemonic), "jp");
    snprintf(lines[1].operands, sizeof(lines[1].operands), "0x1234");
    lines[1].operand_addr = opctx_compute_operand_value(&lines[1]);

    /* Relative displacement: should not substitute. */
    lines[2].rtype = RTYPE_CODE;
    lines[2].byte_count = 2;
    lines[2].bytes[0] = 0x20; lines[2].bytes[1] = 0x08;
    snprintf(lines[2].mnemonic, sizeof(lines[2].mnemonic), "jr");
    snprintf(lines[2].operands, sizeof(lines[2].operands), "nz,$+0x08");
    lines[2].operand_addr = opctx_compute_operand_value(&lines[2]);

    symbols_resolve(lines, 3, sym);

    char out_ops[DISASM_OPERANDS_MAX];
    symbols_format_operands(&lines[0], out_ops, sizeof(out_ops));
    CHECK(strcmp(out_ops, "b,CONST_08") == 0, "constant immediate should be symbolized");
    CHECK(strcmp(lines[0].operands, "b,0x08") == 0, "raw operands must remain unchanged");

    symbols_format_operands(&lines[1], out_ops, sizeof(out_ops));
    CHECK(strcmp(out_ops, "ADDR_1234") == 0, "absolute address should be symbolized");

    symbols_format_operands(&lines[2], out_ops, sizeof(out_ops));
    CHECK(strcmp(out_ops, "nz,$+0x08") == 0, "relative displacement should not be symbolized");

    symbols_free(sym);
    return 0;
}

int main(void) {
    if (test_opctx_core() != 0) return 1;
    if (test_disasm_read_mnm_normalize() != 0) return 1;
    if (test_symbol_substitution_guards() != 0) return 1;

    printf("PASS: z80bench regression tests\n");
    return 0;
}
