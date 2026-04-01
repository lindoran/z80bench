#include "z80bench_core.h"

static int has_relative_displacement_operand(const DisasmLine *dl) {
    if (!dl || dl->rtype != RTYPE_CODE || dl->byte_count < 2)
        return 0;

    unsigned char op0 = dl->bytes[0];
    return (op0 == 0x10 || /* DJNZ e */
            op0 == 0x18 || /* JR e */
            op0 == 0x20 || op0 == 0x28 || op0 == 0x30 || op0 == 0x38); /* JR cc,e */
}

static int has_ixiy_displacement_operand(const DisasmLine *dl) {
    if (!dl || dl->rtype != RTYPE_CODE || dl->byte_count < 3)
        return 0;

    unsigned char op0 = dl->bytes[0];
    if (op0 != 0xDD && op0 != 0xFD)
        return 0;

    unsigned char op1 = dl->bytes[1];
    if (op1 == 0xCB)   /* DD/FD CB d op */
        return dl->byte_count >= 4;

    switch (op1) {
        case 0x34: case 0x35: case 0x36:
        case 0x46: case 0x4E: case 0x56: case 0x5E:
        case 0x66: case 0x6E:
        case 0x70: case 0x71: case 0x72: case 0x73:
        case 0x74: case 0x75: case 0x77:
        case 0x7E:
        case 0x86: case 0x8E: case 0x96: case 0x9E:
        case 0xA6: case 0xAE: case 0xB6: case 0xBE:
            return 1;
        default:
            return 0;
    }
}

int opctx_has_absolute_operand_ref(const DisasmLine *dl) {
    if (!dl || dl->rtype != RTYPE_CODE || dl->byte_count <= 0)
        return 0;

    unsigned char op0 = dl->bytes[0];
    if (op0 == 0xC3 || op0 == 0xCD ||               /* JP nn / CALL nn */
        op0 == 0xC2 || op0 == 0xCA || op0 == 0xD2 || op0 == 0xDA ||
        op0 == 0xE2 || op0 == 0xEA || op0 == 0xF2 || op0 == 0xFA || /* JP cc,nn */
        op0 == 0xC4 || op0 == 0xCC || op0 == 0xD4 || op0 == 0xDC ||
        op0 == 0xE4 || op0 == 0xEC || op0 == 0xF4 || op0 == 0xFC || /* CALL cc,nn */
        op0 == 0x22 || op0 == 0x2A || op0 == 0x32 || op0 == 0x3A || /* LD (nn),HL / HL,(nn) / (nn),A / A,(nn) */
        op0 == 0x01 || op0 == 0x11 || op0 == 0x21 || op0 == 0x31) { /* LD rr,nn */
        return dl->byte_count >= 3;
    }

    if (op0 == 0xED && dl->byte_count >= 4) {
        unsigned char op1 = dl->bytes[1];
        if (op1 == 0x43 || op1 == 0x4B || op1 == 0x53 || op1 == 0x5B ||
            op1 == 0x63 || op1 == 0x6B || op1 == 0x73 || op1 == 0x7B) { /* LD (nn),dd / dd,(nn) */
            return 1;
        }
    }

    if ((op0 == 0xDD || op0 == 0xFD) && dl->byte_count >= 4) {
        unsigned char op1 = dl->bytes[1];
        if (op1 == 0x21 || op1 == 0x22 || op1 == 0x2A) { /* LD IX/IY,nn and (nn),IX/IY forms */
            return 1;
        }
    }

    return 0;
}

int opctx_has_constant_immediate_operand(const DisasmLine *dl) {
    if (!dl || dl->rtype != RTYPE_CODE || dl->byte_count <= 0)
        return 0;

    unsigned char op0 = dl->bytes[0];

    if (op0 == 0x06 || op0 == 0x0E || op0 == 0x16 || op0 == 0x1E ||
        op0 == 0x26 || op0 == 0x2E || op0 == 0x36 || op0 == 0x3E || /* LD r,n and LD (HL),n */
        op0 == 0xC6 || op0 == 0xCE || op0 == 0xD6 || op0 == 0xDE ||
        op0 == 0xE6 || op0 == 0xEE || op0 == 0xF6 || op0 == 0xFE || /* ALU A,n and CP n */
        op0 == 0xD3 || op0 == 0xDB) {                                /* OUT (n),A / IN A,(n) */
        return dl->byte_count >= 2;
    }

    if ((op0 == 0xDD || op0 == 0xFD) && dl->byte_count >= 4) {
        unsigned char op1 = dl->bytes[1];
        if (op1 == 0x36) { /* LD (IX/IY+d),n */
            return 1;
        }
    }

    return 0;
}

int opctx_prefers_byte_hex_width(const DisasmLine *dl) {
    return opctx_has_constant_immediate_operand(dl) ||
           has_relative_displacement_operand(dl) ||
           has_ixiy_displacement_operand(dl);
}

int opctx_compute_operand_value(const DisasmLine *dl) {
    if (!dl || dl->rtype != RTYPE_CODE || dl->byte_count <= 0)
        return -1;

    if (opctx_has_absolute_operand_ref(dl)) {
        unsigned char op0 = dl->bytes[0];
        if (op0 == 0xED && dl->byte_count >= 4) {
            return (int)(dl->bytes[2] | (dl->bytes[3] << 8));
        }
        if ((op0 == 0xDD || op0 == 0xFD) && dl->byte_count >= 4) {
            unsigned char op1 = dl->bytes[1];
            if (op1 == 0x21 || op1 == 0x22 || op1 == 0x2A) {
                return (int)(dl->bytes[2] | (dl->bytes[3] << 8));
            }
        }
        if (dl->byte_count >= 3) {
            return (int)(dl->bytes[1] | (dl->bytes[2] << 8));
        }
        return -1;
    }

    if (opctx_has_constant_immediate_operand(dl)) {
        unsigned char op0 = dl->bytes[0];
        if ((op0 == 0xDD || op0 == 0xFD) && dl->byte_count >= 4 && dl->bytes[1] == 0x36) {
            return (int)dl->bytes[3];
        }
        if (dl->byte_count >= 2) {
            return (int)dl->bytes[1];
        }
    }

    return -1;
}
