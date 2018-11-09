/*
 * Tiny Code Generator for QEMU
 *
 * Copyright (c) 2018 SiFive, Inc
 * Copyright (c) 2008-2009 Arnaud Patard <arnaud.patard@rtp-net.org>
 * Copyright (c) 2009 Aurelien Jarno <aurelien@aurel32.net>
 * Copyright (c) 2008 Fabrice Bellard
 *
 * Based on i386/tcg-target.c and mips/tcg-target.c
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifdef CONFIG_DEBUG_TCG
static const char * const tcg_target_reg_names[TCG_TARGET_NB_REGS] = {
    "zero",
    "ra",
    "sp",
    "gp",
    "tp",
    "t0",
    "t1",
    "t2",
    "s0",
    "s1",
    "a0",
    "a1",
    "a2",
    "a3",
    "a4",
    "a5",
    "a6",
    "a7",
    "s2",
    "s3",
    "s4",
    "s5",
    "s6",
    "s7",
    "s8",
    "s9",
    "s10",
    "s11",
    "t3",
    "t4",
    "t5",
    "t6"
};
#endif

static const int tcg_target_reg_alloc_order[] = {
    /* Call saved registers */
    /* TCG_REG_S0 reservered for TCG_AREG0 */
    TCG_REG_S1,
    TCG_REG_S2,
    TCG_REG_S3,
    TCG_REG_S4,
    TCG_REG_S5,
    TCG_REG_S6,
    TCG_REG_S7,
    TCG_REG_S8,
    TCG_REG_S9,
    TCG_REG_S10,
    TCG_REG_S11,

    /* Call clobbered registers */
    TCG_REG_T0,
    TCG_REG_T1,
    TCG_REG_T2,
    TCG_REG_T3,
    TCG_REG_T4,
    TCG_REG_T5,
    TCG_REG_T6,

    /* Argument registers */
    TCG_REG_A0,
    TCG_REG_A1,
    TCG_REG_A2,
    TCG_REG_A3,
    TCG_REG_A4,
    TCG_REG_A5,
    TCG_REG_A6,
    TCG_REG_A7,
};

static const int tcg_target_call_iarg_regs[] = {
    TCG_REG_A0,
    TCG_REG_A1,
    TCG_REG_A2,
    TCG_REG_A3,
    TCG_REG_A4,
    TCG_REG_A5,
    TCG_REG_A6,
    TCG_REG_A7,
};

static const int tcg_target_call_oarg_regs[] = {
    TCG_REG_A0,
    TCG_REG_A1,
};

#define TCG_CT_CONST_ZERO  0x100
#define TCG_CT_CONST_S12   0x200
#define TCG_CT_CONST_N12   0x400

/* parse target specific constraints */
static const char *target_parse_constraint(TCGArgConstraint *ct,
                                           const char *ct_str, TCGType type)
{
    switch (*ct_str++) {
    case 'r':
        ct->ct |= TCG_CT_REG;
        ct->u.regs = 0xffffffff;
        break;
    case 'L':
        /* qemu_ld/qemu_st constraint */
        ct->ct |= TCG_CT_REG;
        ct->u.regs = 0xffffffff;
        /* qemu_ld/qemu_st uses TCG_REG_TMP0 */
#if defined(CONFIG_SOFTMMU)
        /* tcg_out_tlb_load uses TCG_REG_TMP0/TMP1 and TCG_REG_L0/L1 */
        // tcg_regset_reset_reg(ct->u.regs, TCG_REG_TMP0)
        // tcg_regset_reset_reg(ct->u.regs, TCG_REG_TMP1);
        tcg_regset_reset_reg(ct->u.regs, TCG_REG_TMP2);

        tcg_regset_reset_reg(ct->u.regs, tcg_target_call_iarg_regs[0]);
        tcg_regset_reset_reg(ct->u.regs, tcg_target_call_iarg_regs[1]);
        tcg_regset_reset_reg(ct->u.regs, tcg_target_call_iarg_regs[2]);
        tcg_regset_reset_reg(ct->u.regs, tcg_target_call_iarg_regs[3]);
        tcg_regset_reset_reg(ct->u.regs, tcg_target_call_iarg_regs[4]);
#endif
        break;
    case 'I':
        ct->ct |= TCG_CT_CONST_S12;
        break;
    case 'N':
        ct->ct |= TCG_CT_CONST_N12;
        break;
    case 'Z':
        /* we can use a zero immediate as a zero register argument. */
        ct->ct |= TCG_CT_CONST_ZERO;
        break;
    default:
        return NULL;
    }
    return ct_str;
}

/* test if a constant matches the constraint */
static int tcg_target_const_match(tcg_target_long val, TCGType type,
                                  const TCGArgConstraint *arg_ct)
{
    int ct = arg_ct->ct;
    if (ct & TCG_CT_CONST) {
        return 1;
    }
    if ((ct & TCG_CT_CONST_ZERO) && val == 0) {
        return 1;
    }
    if ((ct & TCG_CT_CONST_S12) && val >= -2048 && val <= 2047) {
        return 1;
    }
    if ((ct & TCG_CT_CONST_N12) && val >= -2047 && val <= 2048) {
        return 1;
    }
    return 0;
}

/*
 * RISC-V Base ISA opcodes (IM)
 */

typedef enum {
    OPC_ADD = 0x33,
    OPC_ADDI = 0x13,
    OPC_ADDIW = 0x1b,
    OPC_ADDW = 0x3b,
    OPC_AND = 0x7033,
    OPC_ANDI = 0x7013,
    OPC_AUIPC = 0x17,
    OPC_BEQ = 0x63,
    OPC_BGE = 0x5063,
    OPC_BGEU = 0x7063,
    OPC_BLT = 0x4063,
    OPC_BLTU = 0x6063,
    OPC_BNE = 0x1063,
    OPC_DIV = 0x2004033,
    OPC_DIVU = 0x2005033,
    OPC_DIVUW = 0x200503b,
    OPC_DIVW = 0x200403b,
    OPC_JAL = 0x6f,
    OPC_JALR = 0x67,
    OPC_LB = 0x3,
    OPC_LBU = 0x4003,
    OPC_LD = 0x3003,
    OPC_LH = 0x1003,
    OPC_LHU = 0x5003,
    OPC_LUI = 0x37,
    OPC_LW = 0x2003,
    OPC_LWU = 0x6003,
    OPC_MUL = 0x2000033,
    OPC_MULH = 0x2001033,
    OPC_MULHSU = 0x2002033,
    OPC_MULHU = 0x2003033,
    OPC_MULW = 0x200003b,
    OPC_OR = 0x6033,
    OPC_ORI = 0x6013,
    OPC_REM = 0x2006033,
    OPC_REMU = 0x2007033,
    OPC_REMUW = 0x200703b,
    OPC_REMW = 0x200603b,
    OPC_SB = 0x23,
    OPC_SD = 0x3023,
    OPC_SH = 0x1023,
    OPC_SLL = 0x1033,
    OPC_SLLI = 0x1013,
    OPC_SLLIW = 0x101b,
    OPC_SLLW = 0x103b,
    OPC_SLT = 0x2033,
    OPC_SLTI = 0x2013,
    OPC_SLTIU = 0x3013,
    OPC_SLTU = 0x3033,
    OPC_SRA = 0x40005033,
    OPC_SRAI = 0x40005013,
    OPC_SRAIW = 0x4000501b,
    OPC_SRAW = 0x4000503b,
    OPC_SRL = 0x5033,
    OPC_SRLI = 0x5013,
    OPC_SRLIW = 0x501b,
    OPC_SRLW = 0x503b,
    OPC_SUB = 0x40000033,
    OPC_SUBW = 0x4000003b,
    OPC_SW = 0x2023,
    OPC_XOR = 0x4033,
    OPC_XORI = 0x4013,
    OPC_FENCE_RW_RW = 0x0330000f,
    OPC_FENCE_R_R = 0x0220000f,
    OPC_FENCE_W_R = 0x0120000f,
    OPC_FENCE_R_W = 0x0210000f,
    OPC_FENCE_W_W = 0x0110000f,
    OPC_FENCE_R_RW = 0x0230000f,
    OPC_FENCE_RW_W = 0x0310000f,
} RISCVInsn;

/*
 * RISC-V immediate and instruction encoders (excludes 16-bit RVC)
 */

/* Type-R */

static int32_t encode_r(RISCVInsn opc, TCGReg rd, TCGReg rs1, TCGReg rs2)
{
    return opc | (rd & 0x1f) << 7 | (rs1 & 0x1f) << 15 | (rs2 & 0x1f) << 20;
}

/* Type-I */

static int32_t encode_imm12(uint32_t imm)
{
    return (imm & 0xfff) << 20;
}

static int32_t encode_i(RISCVInsn opc, TCGReg rd, TCGReg rs1, uint32_t imm)
{
    return opc | (rd & 0x1f) << 7 | (rs1 & 0x1f) << 15 | encode_imm12(imm);
}

/* Type-S */

static int32_t encode_simm12(uint32_t imm)
{
    return ((imm << 20) >> 25) << 25 | ((imm << 27) >> 27) << 7;
}

static int32_t encode_s(RISCVInsn opc, TCGReg rs1, TCGReg rs2, uint32_t imm)
{
    return opc | (rs1 & 0x1f) << 15 | (rs2 & 0x1f) << 20 | encode_simm12(imm);
}

/* Type-SB */

static int32_t encode_sbimm12(uint32_t imm)
{
    return ((imm << 19) >> 31) << 31 | ((imm << 21) >> 26) << 25 |
           ((imm << 27) >> 28) << 8 | ((imm << 20) >> 31) << 7;
}

static int32_t encode_sb(RISCVInsn opc, TCGReg rs1, TCGReg rs2, uint32_t imm)
{
    return opc | (rs1 & 0x1f) << 15 | (rs2 & 0x1f) << 20 | encode_sbimm12(imm);
}

/* Type-U */

static int32_t encode_uimm20(uint32_t imm)
{
    return (imm >> 12) << 12;
}

static int32_t encode_u(RISCVInsn opc, TCGReg rd, uint32_t imm)
{
    return opc | (rd & 0x1f) << 7 | encode_uimm20(imm);
}

/* Type-UJ */

static int32_t encode_ujimm12(uint32_t imm)
{
    return ((imm << 11) >> 31) << 31 | ((imm << 21) >> 22) << 21 |
           ((imm << 20) >> 31) << 20 | ((imm << 12) >> 24) << 12;
}

static int32_t encode_uj(RISCVInsn opc, TCGReg rd, uint32_t imm)
{
    return opc | (rd & 0x1f) << 7 | encode_ujimm12(imm);
}

/*
 * RISC-V instruction emitters
 */

static void tcg_out_opc_reg(TCGContext *s, RISCVInsn opc,
                            TCGReg rd, TCGReg rs1, TCGReg rs2)
{
    tcg_out32(s, encode_r(opc, rd, rs1, rs2));
}

static void tcg_out_opc_imm(TCGContext *s, RISCVInsn opc,
                            TCGReg rd, TCGReg rs1, TCGArg imm)
{
    tcg_out32(s, encode_i(opc, rd, rs1, imm));
}

static void tcg_out_opc_store(TCGContext *s, RISCVInsn opc,
                              TCGReg rs1, TCGReg rs2, uint32_t imm)
{
    tcg_out32(s, encode_s(opc, rs1, rs2, imm));
}

static void tcg_out_opc_branch(TCGContext *s, RISCVInsn opc,
                               TCGReg rs1, TCGReg rs2, uint32_t imm)
{
    tcg_out32(s, encode_sb(opc, rs1, rs2, imm));
}

static void tcg_out_opc_upper(TCGContext *s, RISCVInsn opc,
                              TCGReg rd, uint32_t imm)
{
    tcg_out32(s, encode_u(opc, rd, imm));
}

static void tcg_out_opc_jump(TCGContext *s, RISCVInsn opc,
                             TCGReg rd, uint32_t imm)
{
    tcg_out32(s, encode_uj(opc, rd, imm));
}

/*
 * Relocations
 */

static void reloc_sbimm12(tcg_insn_unit *code_ptr, tcg_insn_unit *target)
{
    intptr_t offset = (intptr_t)target - (intptr_t)code_ptr;
    tcg_debug_assert(offset == sextract32(offset, 1, 12) << 1);

    code_ptr[0] |= encode_sbimm12(offset);
}

static void reloc_jimm20(tcg_insn_unit *code_ptr, tcg_insn_unit *target)
{
    intptr_t offset = (intptr_t)target - (intptr_t)code_ptr;
    tcg_debug_assert(offset == sextract32(offset, 1, 20) << 1);

    code_ptr[0] |= encode_ujimm12(offset);
}

static void reloc_call(tcg_insn_unit *code_ptr, tcg_insn_unit *target)
{
    intptr_t offset = (intptr_t)target - (intptr_t)code_ptr;
    tcg_debug_assert(offset == (int32_t)offset);

    int32_t hi20 = ((offset + 0x800) >> 12) << 12;
    int32_t lo12 = offset - hi20;

    code_ptr[0] |= encode_uimm20(hi20);
    code_ptr[1] |= encode_imm12(lo12);
}

static void patch_reloc(tcg_insn_unit *code_ptr, int type,
                        intptr_t value, intptr_t addend)
{
    tcg_debug_assert(addend == 0);
    switch (type) {
    case R_RISCV_BRANCH:
        reloc_sbimm12(code_ptr, (tcg_insn_unit *)value);
        break;
    case R_RISCV_JAL:
        reloc_jimm20(code_ptr, (tcg_insn_unit *)value);
        break;
    case R_RISCV_CALL:
        reloc_call(code_ptr, (tcg_insn_unit *)value);
        break;
    default:
        tcg_abort();
    }
}

/*
 * TCG intrinsics
 */

static void tcg_out_mov(TCGContext *s, TCGType type, TCGReg ret, TCGReg arg)
{
    if (ret == arg) {
        return;
    }
    switch (type) {
    case TCG_TYPE_I32:
    case TCG_TYPE_I64:
        tcg_out_opc_imm(s, OPC_ADDI, ret, arg, 0);
        break;
    default:
        g_assert_not_reached();
    }
}

static void tcg_out_movi(TCGContext *s, TCGType type, TCGReg rd,
                         tcg_target_long val)
{
    tcg_target_long lo = sextract32(val, 0, 12);
    tcg_target_long hi = val - lo;

    RISCVInsn add32_op = TCG_TARGET_REG_BITS == 64 ? OPC_ADDIW : OPC_ADDI;

#if TCG_TARGET_REG_BITS == 64
    ptrdiff_t offset = tcg_pcrel_diff(s, (void *)val);
#endif

    if (val == lo) {
        tcg_out_opc_imm(s, OPC_ADDI, rd, TCG_REG_ZERO, val);
    } else if (val && !(val & (val - 1))) {
        /* power of 2 */
        tcg_out_opc_imm(s, OPC_ADDI, rd, TCG_REG_ZERO, 1);
        tcg_out_opc_imm(s, OPC_SLLI, rd, rd, ctz64(val));
    } else if (TCG_TARGET_REG_BITS == 64 &&
               !(val >> 31 == 0 || val >> 31 == -1)) {
        int shift = 12 + ctz64(hi >> 12);
        hi >>= shift;
        tcg_out_movi(s, type, rd, hi);
        tcg_out_opc_imm(s, OPC_SLLI, rd, rd, shift);
        if (lo != 0) {
            tcg_out_opc_imm(s, OPC_ADDI, rd, rd, lo);
        }
#if TCG_TARGET_REG_BITS == 64
    } else if (offset == sextract32(offset, 1, 31) << 1) {
        tcg_out_opc_upper(s, OPC_AUIPC, rd, 0);
        tcg_out_opc_imm(s, OPC_ADDI, rd, rd, 0);
        reloc_call(s->code_ptr - 2, (tcg_insn_unit *)val);
#endif
    } else {
        if (hi != 0) {
            tcg_out_opc_upper(s, OPC_LUI, rd, hi);
        }
        if (lo != 0) {
            tcg_out_opc_imm(s, add32_op, rd, hi == 0 ? TCG_REG_ZERO : rd, lo);
        }
    }
}

static void tcg_out_ext8u(TCGContext *s, TCGReg ret, TCGReg arg)
{
    tcg_out_opc_imm(s, OPC_ANDI, ret, arg, 0xff);
}

static void tcg_out_ext16u(TCGContext *s, TCGReg ret, TCGReg arg)
{
    tcg_out_opc_imm(s, OPC_SLLI, ret, arg, TCG_TARGET_REG_BITS - 16);
    tcg_out_opc_imm(s, OPC_SRLI, ret, ret, TCG_TARGET_REG_BITS - 16);
}

static void tcg_out_ext32u(TCGContext *s, TCGReg ret, TCGReg arg)
{
    tcg_out_opc_imm(s, OPC_SLLI, ret, arg, 32);
    tcg_out_opc_imm(s, OPC_SRLI, ret, ret, 32);
}

static void tcg_out_ext8s(TCGContext *s, TCGReg ret, TCGReg arg)
{
    tcg_out_opc_imm(s, OPC_SLLI, ret, arg, TCG_TARGET_REG_BITS - 8);
    tcg_out_opc_imm(s, OPC_SRAI, ret, ret, TCG_TARGET_REG_BITS - 8);
}

static void tcg_out_ext16s(TCGContext *s, TCGReg ret, TCGReg arg)
{
    tcg_out_opc_imm(s, OPC_SLLI, ret, arg, TCG_TARGET_REG_BITS - 16);
    tcg_out_opc_imm(s, OPC_SRAI, ret, ret, TCG_TARGET_REG_BITS - 16);
}

static void tcg_out_ext32s(TCGContext *s, TCGReg ret, TCGReg arg)
{
    tcg_out_opc_imm(s, OPC_ADDIW, ret, arg, 0);
}

static void tcg_out_ldst(TCGContext *s, RISCVInsn opc, TCGReg data,
                         TCGReg addr, intptr_t offset)
{
    int32_t imm12 = sextract32(offset, 0, 12);
    if (offset != imm12) {
        tcg_out_movi(s, TCG_TYPE_PTR, TCG_REG_TMP2, offset - imm12);
        if (addr != TCG_REG_ZERO) {
            tcg_out_opc_reg(s, OPC_ADD, TCG_REG_TMP2, TCG_REG_TMP2, addr);
        }
        addr = TCG_REG_TMP2;
    }
    switch (opc) {
    case OPC_SB:
    case OPC_SH:
    case OPC_SW:
    case OPC_SD:
        tcg_out_opc_store(s, opc, addr, data, imm12);
        break;
    case OPC_LB:
    case OPC_LBU:
    case OPC_LH:
    case OPC_LHU:
    case OPC_LW:
    case OPC_LWU:
    case OPC_LD:
        tcg_out_opc_imm(s, opc, data, addr, imm12);
        break;
    default:
        g_assert_not_reached();
    }
}

static void tcg_out_ld(TCGContext *s, TCGType type, TCGReg arg,
                       TCGReg arg1, intptr_t arg2)
{
    bool is32bit = (TCG_TARGET_REG_BITS == 32 || type == TCG_TYPE_I32);
    tcg_out_ldst(s, is32bit ? OPC_LW : OPC_LD, arg, arg1, arg2);
}

static void tcg_out_st(TCGContext *s, TCGType type, TCGReg arg,
                       TCGReg arg1, intptr_t arg2)
{
    bool is32bit = (TCG_TARGET_REG_BITS == 32 || type == TCG_TYPE_I32);
    tcg_out_ldst(s, is32bit ? OPC_SW : OPC_SD, arg, arg1, arg2);
}

static bool tcg_out_sti(TCGContext *s, TCGType type, TCGArg val,
                        TCGReg base, intptr_t ofs)
{
    if (val == 0) {
        tcg_out_st(s, type, TCG_REG_ZERO, base, ofs);
        return true;
    }
    return false;
}

void tb_target_set_jmp_target(uintptr_t tc_ptr, uintptr_t jmp_addr,
                              uintptr_t addr)
{
    /* Note: jump target patching should be atomic */
    reloc_call((tcg_insn_unit *)jmp_addr, (tcg_insn_unit*)addr);
    flush_icache_range(jmp_addr, jmp_addr + 8);
}

typedef struct {
    DebugFrameHeader h;
    uint8_t fde_def_cfa[4];
    uint8_t fde_reg_ofs[ARRAY_SIZE(tcg_target_callee_save_regs) * 2];
} DebugFrame;

#define ELF_HOST_MACHINE EM_RISCV

static const DebugFrame debug_frame = {
    .h.cie.len = sizeof(DebugFrameCIE) - 4, /* length after .len member */
    .h.cie.id = -1,
    .h.cie.version = 1,
    .h.cie.code_align = 1,
    .h.cie.data_align = -(TCG_TARGET_REG_BITS / 8) & 0x7f, /* sleb128 */
    .h.cie.return_column = TCG_REG_RA,

    /* Total FDE size does not include the "len" member.  */
    .h.fde.len = sizeof(DebugFrame) - offsetof(DebugFrame, h.fde.cie_offset),

    .fde_def_cfa = {
        12, TCG_REG_SP,                 /* DW_CFA_def_cfa sp, ... */
        (FRAME_SIZE & 0x7f) | 0x80,     /* ... uleb128 FRAME_SIZE */
        (FRAME_SIZE >> 7)
    },
    .fde_reg_ofs = {
        0x80 + 9,  12,                  /* DW_CFA_offset, s1,  -96 */
        0x80 + 18, 11,                  /* DW_CFA_offset, s2,  -88 */
        0x80 + 19, 10,                  /* DW_CFA_offset, s3,  -80 */
        0x80 + 20, 9,                   /* DW_CFA_offset, s4,  -72 */
        0x80 + 21, 8,                   /* DW_CFA_offset, s5,  -64 */
        0x80 + 22, 7,                   /* DW_CFA_offset, s6,  -56 */
        0x80 + 23, 6,                   /* DW_CFA_offset, s7,  -48 */
        0x80 + 24, 5,                   /* DW_CFA_offset, s8,  -40 */
        0x80 + 25, 4,                   /* DW_CFA_offset, s9,  -32 */
        0x80 + 26, 3,                   /* DW_CFA_offset, s10, -24 */
        0x80 + 27, 2,                   /* DW_CFA_offset, s11, -16 */
        0x80 + 1 , 1,                   /* DW_CFA_offset, ra,  -8 */
    }
};

void tcg_register_jit(void *buf, size_t buf_size)
{
    tcg_register_jit_int(buf, buf_size, &debug_frame, sizeof(debug_frame));
}
