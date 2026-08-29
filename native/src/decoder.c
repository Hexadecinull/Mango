#include "mango/decoder.h"

#include "mango/cpu.h"

int mango_decode(uint32_t word, MangoInsn* out) {
  out->cond = (word >> 28) & 0xF;
  out->rd = out->rn = out->rm = out->rs = out->imm = 0;
  out->shift_type = 0;
  out->shift_amount = 0;
  out->is_imm = 0;
  out->sets_flags = 0;
  out->u = 0;
  out->b = 0;
  out->op = MANGO_OP_UNKNOWN;

  if (out->cond == 0xF) {
    return -1; /* 0xF is ARMv5+'s unconditional-extension selector, not a real cond */
  }

  /* SVC/SWI: bits 27-24 = 1111, the rest is a legacy immediate EABI code
   * ignores; the actual syscall number is in r7 at execution time, not
   * decoded here, see mango_interp_run's SVC case in interp.c. */
  if (((word >> 24) & 0xF) == 0xF) {
    out->op = MANGO_OP_SVC;
    return 0;
  }

  /* BX Rm: cond 0001 0010 1111 1111 1111 0001 Rm */
  if (((word >> 20) & 0xFF) == 0x12 && ((word >> 4) & 0xFFFF) == 0xFFF1) {
    out->op = MANGO_OP_BX;
    out->rm = word & 0xF;
    return 0;
  }

  /* B/BL imm24: bits 27-25 = 101, bit 24 = L (0=B, 1=BL) */
  if (((word >> 25) & 0x7) == 0x5) {
    uint32_t l = (word >> 24) & 0x1;
    uint32_t imm24 = word & 0xFFFFFF;
    uint32_t offset;
    if (imm24 & 0x800000) {
      offset = (imm24 | 0xFF000000u) << 2;
    } else {
      offset = imm24 << 2;
    }
    out->op = l ? MANGO_OP_BL : MANGO_OP_B;
    out->imm = offset; /* two's complement offset, added as unsigned */
    return 0;
  }

  /* MUL Rd,Rm,Rs: cond 000000 A S Rd 0000 Rs 1001 Rm. Same bits27-26 as
   * data-processing below, so this must be checked first or AND/EOR would
   * silently steal it (their opcodes are 0000/0001, exactly A/S here). */
  if (((word >> 22) & 0x3F) == 0x0 && ((word >> 4) & 0xF) == 0x9) {
    uint32_t a = (word >> 21) & 0x1;
    uint32_t s = (word >> 20) & 0x1;
    uint32_t rd = (word >> 16) & 0xF;
    uint32_t rn_field = (word >> 12) & 0xF;
    uint32_t rs = (word >> 8) & 0xF;
    uint32_t rm = word & 0xF;
    if (a) {
      return -1; /* MLA, not supported yet */
    }
    if (rn_field != 0 || rd == MANGO_REG_PC || rm == MANGO_REG_PC || rs == MANGO_REG_PC) {
      return -1; /* SBZ violated, or PC as an operand, both UNPREDICTABLE */
    }
    out->op = MANGO_OP_MUL;
    out->rd = rd;
    out->rm = rm;
    out->rs = rs;
    out->sets_flags = (int)s;
    return 0;
  }

  /* Data-processing: bits 27-26 == 00 */
  if (((word >> 26) & 0x3) == 0x0) {
    uint32_t i = (word >> 25) & 0x1;
    uint32_t opcode = (word >> 21) & 0xF;
    uint32_t s = (word >> 20) & 0x1;
    uint32_t rn = (word >> 16) & 0xF;
    uint32_t rd = (word >> 12) & 0xF;
    uint32_t operand2 = word & 0xFFF;
    MangoOp op;

    switch (opcode) {
      case 0x0:
        op = MANGO_OP_AND;
        break;
      case 0x1:
        op = MANGO_OP_EOR;
        break;
      case 0x2:
        op = MANGO_OP_SUB;
        break;
      case 0x3:
        op = MANGO_OP_RSB;
        break;
      case 0x4:
        op = MANGO_OP_ADD;
        break;
      case 0x5:
        op = MANGO_OP_ADC;
        break;
      case 0x6:
        op = MANGO_OP_SBC;
        break;
      case 0x7:
        op = MANGO_OP_RSC;
        break;
      case 0x8:
      case 0x9:
      case 0xA:
      case 0xB:
        /* TST/TEQ/CMP/CMN: S=0 here isn't one of these at all (it overlaps
         * MRS/MSR's encoding instead), so decode nothing rather than
         * guess. */
        if (!s) {
          return -1;
        }
        op = opcode == 0x8   ? MANGO_OP_TST
             : opcode == 0x9 ? MANGO_OP_TEQ
             : opcode == 0xA ? MANGO_OP_CMP
                             : MANGO_OP_CMN;
        break;
      case 0xC:
        op = MANGO_OP_ORR;
        break;
      case 0xD:
        op = MANGO_OP_MOV;
        break;
      case 0xE:
        op = MANGO_OP_BIC;
        break;
      case 0xF:
        op = MANGO_OP_MVN;
        break;
      default:
        return -1;
    }

    out->op = op;
    out->rn = rn;
    out->rd = rd;
    out->sets_flags = (int)s;

    if (i) {
      uint32_t imm8 = operand2 & 0xFF;
      uint32_t rot = ((operand2 >> 8) & 0xF) * 2;
      uint32_t val = imm8;
      if (rot != 0) {
        val = (imm8 >> rot) | (imm8 << (32 - rot));
      }
      out->is_imm = 1;
      out->imm = val;
    } else {
      uint32_t shift_by_reg = (operand2 >> 4) & 0x1;
      if (shift_by_reg) {
        return -1; /* register-specified shift amount not supported yet */
      }
      uint32_t shift_type = (operand2 >> 5) & 0x3;
      uint32_t shift_amount = (operand2 >> 7) & 0x1F;
      if (shift_type != 0 && shift_amount == 0) {
        return -1; /* #0 means #32 for LSR/ASR, RRX for ROR, see decoder.h */
      }
      out->rm = operand2 & 0xF;
      out->shift_type = shift_type;
      out->shift_amount = shift_amount;
    }
    return 0;
  }

  /* LDR/STR immediate offset, no writeback: bits 27-26=01, I=0, P=1, W=0 */
  if (((word >> 26) & 0x3) == 0x1 && ((word >> 25) & 0x1) == 0 && ((word >> 24) & 0x1) == 1 &&
      ((word >> 21) & 0x1) == 0) {
    uint32_t u = (word >> 23) & 0x1;
    uint32_t b = (word >> 22) & 0x1;
    uint32_t l = (word >> 20) & 0x1;
    uint32_t rn = (word >> 16) & 0xF;
    uint32_t rt = (word >> 12) & 0xF;
    uint32_t imm12 = word & 0xFFF;

    out->op = l ? MANGO_OP_LDR : MANGO_OP_STR;
    out->rn = rn;
    out->rd = rt;
    out->imm = imm12;
    out->u = (int)u;
    out->b = (int)b;
    return 0;
  }

  return -1;
}
