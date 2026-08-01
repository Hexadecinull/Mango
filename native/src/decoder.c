#include "mango/decoder.h"

int mango_decode(uint32_t word, MangoInsn* out) {
  out->cond = (word >> 28) & 0xF;
  out->rd = out->rn = out->rm = out->imm = 0;
  out->is_imm = 0;
  out->sets_flags = 0;
  out->u = 0;
  out->op = MANGO_OP_UNKNOWN;

  if (out->cond != 0xE) {
    return -1; /* only AL handled in this proof of concept */
  }

  /* BX Rm: cond 0001 0010 1111 1111 1111 0001 Rm */
  if (((word >> 20) & 0xFF) == 0x12 && ((word >> 4) & 0xFFFF) == 0xFFF1) {
    out->op = MANGO_OP_BX;
    out->rm = word & 0xF;
    return 0;
  }

  /* B imm24: bits 27-25 = 101, bit 24 (L) = 0 for a plain B, not BL. */
  if (((word >> 25) & 0x7) == 0x5) {
    if (((word >> 24) & 0x1) != 0) {
      return -1; /* BL not handled yet */
    }
    uint32_t imm24 = word & 0xFFFFFF;
    uint32_t offset;
    if (imm24 & 0x800000) {
      offset = (imm24 | 0xFF000000u) << 2;
    } else {
      offset = imm24 << 2;
    }
    out->op = MANGO_OP_B;
    out->imm = offset; /* two's complement offset, added as unsigned */
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
      case 0x4:
        op = MANGO_OP_ADD;
        break;
      case 0x2:
        op = MANGO_OP_SUB;
        break;
      case 0xD:
        op = MANGO_OP_MOV;
        break;
      case 0xA:
        op = MANGO_OP_CMP;
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
      if (((operand2 >> 4) & 0xFF) != 0) {
        return -1; /* shifted register operand2 not supported yet */
      }
      out->rm = operand2 & 0xF;
    }
    return 0;
  }

  /* LDR/STR immediate, offset addressing: bits 27-26 == 01, I(25)=0
   * (immediate, not register offset), P(24)=1 (pre-indexed), W(21)=0
   * (no writeback), B(22)=0 (word access, not byte). Anything outside
   * this narrow shape (register-offset, post-indexed, writeback, byte
   * access) isn't decoded yet, see decoder.h. */
  if (((word >> 26) & 0x3) == 0x1 && ((word >> 25) & 0x1) == 0 &&
      ((word >> 24) & 0x1) == 1 && ((word >> 22) & 0x1) == 0 &&
      ((word >> 21) & 0x1) == 0) {
    uint32_t u = (word >> 23) & 0x1;
    uint32_t l = (word >> 20) & 0x1;
    uint32_t rn = (word >> 16) & 0xF;
    uint32_t rt = (word >> 12) & 0xF;
    uint32_t imm12 = word & 0xFFF;

    out->op = l ? MANGO_OP_LDR : MANGO_OP_STR;
    out->rn = rn;
    out->rd = rt;
    out->imm = imm12;
    out->u = (int)u;
    return 0;
  }

  return -1;
}
