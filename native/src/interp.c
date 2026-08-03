#include "mango/interp.h"

#include "mango/decoder.h"

/* Little-endian, matching real ARM32 Android/Linux. Written as explicit
 * byte shifts rather than a pointer cast, so this doesn't depend on host
 * endianness or alignment and doesn't trip strict-aliasing. */
static uint32_t mango_load_u32_le(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void mango_store_u32_le(uint8_t* p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xFFu);
  p[1] = (uint8_t)((v >> 8) & 0xFFu);
  p[2] = (uint8_t)((v >> 16) & 0xFFu);
  p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

/* 0 on success, -1 if [addr, addr+4) isn't a valid, aligned word access. */
static int mango_check_word_access(const MangoMemory* mem, uint32_t addr) {
  if ((addr % 4u) != 0) {
    return -1;
  }
  /* addr+4 as uint64_t so a near-UINT32_MAX addr can't wrap and pass. */
  if ((uint64_t)addr + 4u > mem->size) {
    return -1;
  }
  return 0;
}

/* 0 on success, -1 if addr isn't a valid byte access. No alignment
 * requirement, every address is a valid byte offset. */
static int mango_check_byte_access(const MangoMemory* mem, uint32_t addr) {
  return addr < mem->size ? 0 : -1;
}

/* Real hardware: reading r15 (PC) as an operand gives the current
 * instruction's address + 8, not whatever cpu->r[15] currently holds
 * (that's only updated to +4 after each instruction executes, see the
 * loop below). This matters in practice: PC-relative loads of literal
 * pool constants are everywhere in real compiled ARM code. */
static uint32_t mango_read_reg(const MangoCpu* cpu, uint32_t insn_addr, uint32_t reg) {
  if (reg == MANGO_REG_PC) {
    return insn_addr + 8u;
  }
  return cpu->r[reg];
}

/* shift_amount is always 1-31 for LSR/ASR/ROR and 0-31 for LSL, decode
 * rejects everything else (the LSR/ASR "#0 means #32" and ROR "#0 means
 * RRX" special cases), so every shift here is well-defined with no risk
 * of a >=32 shift, which would be undefined behavior in C. ASR is
 * written out by hand rather than relying on `(int32_t)value >>
 * shift_amount`, since right-shifting a negative signed value is only
 * implementation-defined in C, not guaranteed portable. */
static uint32_t mango_apply_shift(uint32_t value, uint32_t shift_type, uint32_t shift_amount) {
  switch (shift_type) {
    case 0: /* LSL */
      return shift_amount == 0 ? value : value << shift_amount;
    case 1: /* LSR */
      return value >> shift_amount;
    case 2: { /* ASR */
      uint32_t sign_fill = (value & 0x80000000u) ? (~0u << (32 - shift_amount)) : 0u;
      return (value >> shift_amount) | sign_fill;
    }
    case 3: /* ROR */
      return (value >> shift_amount) | (value << (32 - shift_amount));
    default:
      return value;
  }
}

/* operand2's actual value: the immediate as-is, or the shifted register. */
static uint32_t mango_read_operand2(const MangoCpu* cpu, uint32_t addr, const MangoInsn* insn) {
  if (insn->is_imm) {
    return insn->imm;
  }
  uint32_t value = mango_read_reg(cpu, addr, insn->rm);
  return mango_apply_shift(value, insn->shift_type, insn->shift_amount);
}

/* NZCV for an ADD-style operation (ADD, ADDS): result = lhs + rhs. */
static uint32_t mango_flags_for_add(uint32_t lhs, uint32_t rhs, uint32_t result) {
  uint32_t flags = 0;
  flags |= (result == 0) ? MANGO_CPSR_Z : 0;
  flags |= (result & 0x80000000u) ? MANGO_CPSR_N : 0;
  flags |= (result < lhs) ? MANGO_CPSR_C : 0; /* wrapped past UINT32_MAX: carry out */
  /* Signed overflow: operands had the same sign, and the result's sign
   * doesn't match. */
  flags |= ((~(lhs ^ rhs) & (lhs ^ result)) & 0x80000000u) ? MANGO_CPSR_V : 0;
  return flags;
}

/* NZCV for a SUB-style operation (SUB, SUBS, CMP): result = lhs - rhs. */
static uint32_t mango_flags_for_sub(uint32_t lhs, uint32_t rhs, uint32_t result) {
  uint32_t flags = 0;
  flags |= (result == 0) ? MANGO_CPSR_Z : 0;
  flags |= (result & 0x80000000u) ? MANGO_CPSR_N : 0;
  flags |= (lhs >= rhs) ? MANGO_CPSR_C : 0; /* C = NOT borrow */
  /* Signed overflow: operands had different signs, and the result's
   * sign doesn't match the minuend's. */
  flags |= ((lhs ^ rhs) & (lhs ^ result) & 0x80000000u) ? MANGO_CPSR_V : 0;
  return flags;
}

static void mango_set_nzcv(MangoCpu* cpu, uint32_t flags) {
  cpu->cpsr &= ~(MANGO_CPSR_N | MANGO_CPSR_Z | MANGO_CPSR_C | MANGO_CPSR_V);
  cpu->cpsr |= flags;
}

/* Whether a conditionally-executed instruction with this cond code
 * should actually run, given the current NZCV flags. Standard ARM
 * condition table; cond=0xE (AL) and 0xF (rejected at decode) never
 * reach here. An unrecognized value shouldn't be reachable either, but
 * defaults to "don't execute" rather than "always execute" if it is. */
static int mango_cond_holds(uint32_t cond, uint32_t cpsr) {
  int n = (cpsr & MANGO_CPSR_N) != 0;
  int z = (cpsr & MANGO_CPSR_Z) != 0;
  int c = (cpsr & MANGO_CPSR_C) != 0;
  int v = (cpsr & MANGO_CPSR_V) != 0;

  switch (cond) {
    case 0x0:
      return z; /* EQ: equal */
    case 0x1:
      return !z; /* NE: not equal */
    case 0x2:
      return c; /* CS/HS: unsigned >= */
    case 0x3:
      return !c; /* CC/LO: unsigned < */
    case 0x4:
      return n; /* MI: negative */
    case 0x5:
      return !n; /* PL: positive or zero */
    case 0x6:
      return v; /* VS: signed overflow */
    case 0x7:
      return !v; /* VC: no signed overflow */
    case 0x8:
      return c && !z; /* HI: unsigned > */
    case 0x9:
      return !c || z; /* LS: unsigned <= */
    case 0xA:
      return n == v; /* GE: signed >= */
    case 0xB:
      return n != v; /* LT: signed < */
    case 0xC:
      return !z && (n == v); /* GT: signed > */
    case 0xD:
      return z || (n != v); /* LE: signed <= */
    case 0xE:
      return 1; /* AL: always */
    default:
      return 0;
  }
}

int mango_interp_run(MangoCpu* cpu, MangoMemory* mem, uint32_t max_steps) {
  uint32_t addr = cpu->r[MANGO_REG_PC];

  for (uint32_t step = 0; step < max_steps; step++) {
    if (mango_check_word_access(mem, addr) != 0) {
      return -1;
    }

    MangoInsn insn;
    if (mango_decode(mango_load_u32_le(mem->bytes + addr), &insn) != 0) {
      return -1;
    }

    uint32_t next_addr = addr + 4u;

    /* A conditionally-skipped instruction is a no-op: PC still advances
     * normally (or, for a skipped B, the branch just isn't taken), but
     * none of its effects happen. This one check covers every opcode,
     * including B and BX, rather than needing per-case handling. */
    if (mango_cond_holds(insn.cond, cpu->cpsr)) {
      switch (insn.op) {
        case MANGO_OP_MOV: {
          uint32_t result = mango_read_operand2(cpu, addr, &insn);
          cpu->r[insn.rd] = result;
          if (insn.sets_flags) {
            /* MOVS should set C from the shifter's carry-out when
             * operand2 involves a shift (LSL/LSR/ASR/ROR by nonzero),
             * not touch C at all otherwise. Only the "otherwise" half
             * is implemented: C is always just preserved here, which
             * is correct for a plain MOVS/MOVS with LSL #0, and wrong
             * for e.g. `movs r0, r1, lsl #5`. ADD/SUB/CMP don't have
             * this gap, their C comes from the arithmetic itself, not
             * the shifter. */
            uint32_t n = (result & 0x80000000u) ? MANGO_CPSR_N : 0;
            uint32_t z = (result == 0) ? MANGO_CPSR_Z : 0;
            mango_set_nzcv(cpu, (cpu->cpsr & MANGO_CPSR_C) | n | z);
          }
          break;
        }

        case MANGO_OP_ADD: {
          uint32_t rhs = mango_read_operand2(cpu, addr, &insn);
          uint32_t lhs = mango_read_reg(cpu, addr, insn.rn);
          uint32_t result = lhs + rhs;
          cpu->r[insn.rd] = result;
          if (insn.sets_flags) {
            mango_set_nzcv(cpu, mango_flags_for_add(lhs, rhs, result));
          }
          break;
        }

        case MANGO_OP_SUB: {
          uint32_t rhs = mango_read_operand2(cpu, addr, &insn);
          uint32_t lhs = mango_read_reg(cpu, addr, insn.rn);
          uint32_t result = lhs - rhs;
          cpu->r[insn.rd] = result;
          if (insn.sets_flags) {
            mango_set_nzcv(cpu, mango_flags_for_sub(lhs, rhs, result));
          }
          break;
        }

        case MANGO_OP_CMP: {
          uint32_t rhs = mango_read_operand2(cpu, addr, &insn);
          uint32_t lhs = mango_read_reg(cpu, addr, insn.rn);
          mango_set_nzcv(cpu, mango_flags_for_sub(lhs, rhs, lhs - rhs));
          break;
        }

        case MANGO_OP_B:
          /* insn.imm is a two's complement offset; PC reads as addr+8. */
          next_addr = addr + 8u + insn.imm;
          break;

        case MANGO_OP_BX:
          cpu->r[MANGO_REG_PC] = cpu->r[insn.rm];
          return 0;

        case MANGO_OP_LDR:
        case MANGO_OP_STR: {
          if (insn.rd == MANGO_REG_PC) {
            /* Loading a new PC is an indirect branch, not supported yet. */
            return -1;
          }
          uint32_t base = mango_read_reg(cpu, addr, insn.rn);
          uint32_t eaddr = insn.u ? base + insn.imm : base - insn.imm;

          if (insn.b) {
            if (mango_check_byte_access(mem, eaddr) != 0) {
              return -1;
            }
            if (insn.op == MANGO_OP_LDR) {
              cpu->r[insn.rd] = mem->bytes[eaddr]; /* zero-extended, no LDRSB */
            } else {
              mem->bytes[eaddr] = (uint8_t)(cpu->r[insn.rd] & 0xFFu);
            }
          } else {
            if (mango_check_word_access(mem, eaddr) != 0) {
              return -1;
            }
            if (insn.op == MANGO_OP_LDR) {
              cpu->r[insn.rd] = mango_load_u32_le(mem->bytes + eaddr);
            } else {
              mango_store_u32_le(mem->bytes + eaddr, cpu->r[insn.rd]);
            }
          }
          break;
        }

        default:
          return -1;
      }
    }

    addr = next_addr;
    cpu->r[MANGO_REG_PC] = addr;
  }

  return -1; /* step limit hit, most likely an infinite loop in the guest */
}
