#include "mango/interp.h"

#include "mango/decoder.h"

/* Little-endian (real ARM32 Android/Linux), explicit shifts to stay strict-aliasing-safe. */
static uint32_t mango_load_u32_le(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void mango_store_u32_le(uint8_t* p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xFFu);
  p[1] = (uint8_t)((v >> 8) & 0xFFu);
  p[2] = (uint8_t)((v >> 16) & 0xFFu);
  p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static int mango_check_word_access(const MangoMemory* mem, uint32_t addr) {
  if ((addr % 4u) != 0) {
    return -1;
  }
  if ((uint64_t)addr + 4u > mem->size) { /* uint64_t so addr near UINT32_MAX can't wrap */
    return -1;
  }
  return 0;
}

static int mango_check_byte_access(const MangoMemory* mem, uint32_t addr) {
  return addr < mem->size ? 0 : -1;
}

/* Real hardware: reading PC as an operand gives addr+8, not cpu->r[15]. */
static uint32_t mango_read_reg(const MangoCpu* cpu, uint32_t insn_addr, uint32_t reg) {
  if (reg == MANGO_REG_PC) {
    return insn_addr + 8u;
  }
  return cpu->r[reg];
}

/* shift_amount is always in a safe 0-31 (LSL) or 1-31 (LSR/ASR/ROR) range, decode
 * guarantees it. ASR is hand-rolled since signed right-shift is implementation-defined. */
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

static uint32_t mango_read_operand2(const MangoCpu* cpu, uint32_t addr, const MangoInsn* insn) {
  if (insn->is_imm) {
    return insn->imm;
  }
  uint32_t value = mango_read_reg(cpu, addr, insn->rm);
  return mango_apply_shift(value, insn->shift_type, insn->shift_amount);
}

/* NZCV for ADD/ADDS: result = lhs + rhs. */
static uint32_t mango_flags_for_add(uint32_t lhs, uint32_t rhs, uint32_t result) {
  uint32_t flags = 0;
  flags |= (result == 0) ? MANGO_CPSR_Z : 0;
  flags |= (result & 0x80000000u) ? MANGO_CPSR_N : 0;
  flags |= (result < lhs) ? MANGO_CPSR_C : 0;
  flags |= ((~(lhs ^ rhs) & (lhs ^ result)) & 0x80000000u) ? MANGO_CPSR_V : 0;
  return flags;
}

/* NZCV for SUB/SUBS/CMP: result = lhs - rhs. */
static uint32_t mango_flags_for_sub(uint32_t lhs, uint32_t rhs, uint32_t result) {
  uint32_t flags = 0;
  flags |= (result == 0) ? MANGO_CPSR_Z : 0;
  flags |= (result & 0x80000000u) ? MANGO_CPSR_N : 0;
  flags |= (lhs >= rhs) ? MANGO_CPSR_C : 0;
  flags |= ((lhs ^ rhs) & (lhs ^ result) & 0x80000000u) ? MANGO_CPSR_V : 0;
  return flags;
}

/* NZCV for ADC (result = lhs + rhs + carry_in); SBC/RSC reuse this too,
 * feeding it ~rhs (see their cases below), the same trick real ALU
 * hardware uses since A-B-1+C == A+~B+C in two's complement. */
static uint32_t mango_flags_for_adc(uint32_t lhs, uint32_t rhs, uint32_t carry_in, uint32_t result) {
  uint32_t flags = 0;
  uint64_t wide = (uint64_t)lhs + rhs + carry_in;
  flags |= (result == 0) ? MANGO_CPSR_Z : 0;
  flags |= (result & 0x80000000u) ? MANGO_CPSR_N : 0;
  flags |= (wide > 0xFFFFFFFFu) ? MANGO_CPSR_C : 0;
  flags |= ((~(lhs ^ rhs) & (lhs ^ result)) & 0x80000000u) ? MANGO_CPSR_V : 0;
  return flags;
}

/* NZ for AND/EOR/ORR/BIC/MVN/TST/TEQ: C should come from the shifter, not
 * implemented (see MOVS below), and V is unaffected by these; both kept
 * as-is rather than cleared. */
static uint32_t mango_flags_for_logical(const MangoCpu* cpu, uint32_t result) {
  uint32_t n = (result & 0x80000000u) ? MANGO_CPSR_N : 0;
  uint32_t z = (result == 0) ? MANGO_CPSR_Z : 0;
  return (cpu->cpsr & (MANGO_CPSR_C | MANGO_CPSR_V)) | n | z;
}

static void mango_set_nzcv(MangoCpu* cpu, uint32_t flags) {
  cpu->cpsr &= ~(MANGO_CPSR_N | MANGO_CPSR_Z | MANGO_CPSR_C | MANGO_CPSR_V);
  cpu->cpsr |= flags;
}

static int mango_cond_holds(uint32_t cond, uint32_t cpsr) {
  int n = (cpsr & MANGO_CPSR_N) != 0;
  int z = (cpsr & MANGO_CPSR_Z) != 0;
  int c = (cpsr & MANGO_CPSR_C) != 0;
  int v = (cpsr & MANGO_CPSR_V) != 0;

  switch (cond) {
    case 0x0:
      return z; /* EQ */
    case 0x1:
      return !z; /* NE */
    case 0x2:
      return c; /* CS/HS */
    case 0x3:
      return !c; /* CC/LO */
    case 0x4:
      return n; /* MI */
    case 0x5:
      return !n; /* PL */
    case 0x6:
      return v; /* VS */
    case 0x7:
      return !v; /* VC */
    case 0x8:
      return c && !z; /* HI */
    case 0x9:
      return !c || z; /* LS */
    case 0xA:
      return n == v; /* GE */
    case 0xB:
      return n != v; /* LT */
    case 0xC:
      return !z && (n == v); /* GT */
    case 0xD:
      return z || (n != v); /* LE */
    case 0xE:
      return 1; /* AL */
    default:
      return 0;
  }
}

int mango_interp_run(MangoCpu* cpu, MangoMemory* mem, uint32_t stop_addr, uint32_t max_steps) {
  uint32_t addr = cpu->r[MANGO_REG_PC];

  for (uint32_t step = 0; step < max_steps; step++) {
    if (addr == stop_addr) {
      return 0;
    }
    if (mango_check_word_access(mem, addr) != 0) {
      return -1;
    }

    MangoInsn insn;
    if (mango_decode(mango_load_u32_le(mem->bytes + addr), &insn) != 0) {
      return -1;
    }

    uint32_t next_addr = addr + 4u;

    /* condition false = no-op, covers B/BX too, no per-case handling needed */
    if (mango_cond_holds(insn.cond, cpu->cpsr)) {
      switch (insn.op) {
        case MANGO_OP_MOV: {
          uint32_t result = mango_read_operand2(cpu, addr, &insn);
          cpu->r[insn.rd] = result;
          if (insn.sets_flags) {
            mango_set_nzcv(cpu, mango_flags_for_logical(cpu, result));
          }
          break;
        }

        case MANGO_OP_MVN: {
          uint32_t result = ~mango_read_operand2(cpu, addr, &insn);
          cpu->r[insn.rd] = result;
          if (insn.sets_flags) {
            mango_set_nzcv(cpu, mango_flags_for_logical(cpu, result));
          }
          break;
        }

        case MANGO_OP_AND:
        case MANGO_OP_EOR:
        case MANGO_OP_ORR:
        case MANGO_OP_BIC: {
          uint32_t rhs = mango_read_operand2(cpu, addr, &insn);
          uint32_t lhs = mango_read_reg(cpu, addr, insn.rn);
          uint32_t result = insn.op == MANGO_OP_AND   ? (lhs & rhs)
                             : insn.op == MANGO_OP_EOR ? (lhs ^ rhs)
                             : insn.op == MANGO_OP_ORR ? (lhs | rhs)
                                                        : (lhs & ~rhs);
          cpu->r[insn.rd] = result;
          if (insn.sets_flags) {
            mango_set_nzcv(cpu, mango_flags_for_logical(cpu, result));
          }
          break;
        }

        case MANGO_OP_TST:
        case MANGO_OP_TEQ: {
          uint32_t rhs = mango_read_operand2(cpu, addr, &insn);
          uint32_t lhs = mango_read_reg(cpu, addr, insn.rn);
          uint32_t result = insn.op == MANGO_OP_TST ? (lhs & rhs) : (lhs ^ rhs);
          mango_set_nzcv(cpu, mango_flags_for_logical(cpu, result));
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

        case MANGO_OP_CMN: {
          uint32_t rhs = mango_read_operand2(cpu, addr, &insn);
          uint32_t lhs = mango_read_reg(cpu, addr, insn.rn);
          mango_set_nzcv(cpu, mango_flags_for_add(lhs, rhs, lhs + rhs));
          break;
        }

        case MANGO_OP_ADC: {
          uint32_t rhs = mango_read_operand2(cpu, addr, &insn);
          uint32_t lhs = mango_read_reg(cpu, addr, insn.rn);
          uint32_t carry_in = (cpu->cpsr & MANGO_CPSR_C) ? 1u : 0u;
          uint32_t result = lhs + rhs + carry_in;
          cpu->r[insn.rd] = result;
          if (insn.sets_flags) {
            mango_set_nzcv(cpu, mango_flags_for_adc(lhs, rhs, carry_in, result));
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

        case MANGO_OP_RSB: {
          uint32_t rhs = mango_read_operand2(cpu, addr, &insn);
          uint32_t lhs = mango_read_reg(cpu, addr, insn.rn);
          uint32_t result = rhs - lhs;
          cpu->r[insn.rd] = result;
          if (insn.sets_flags) {
            mango_set_nzcv(cpu, mango_flags_for_sub(rhs, lhs, result));
          }
          break;
        }

        case MANGO_OP_SBC: {
          uint32_t rhs = ~mango_read_operand2(cpu, addr, &insn); /* A-B-1+C == A+~B+C */
          uint32_t lhs = mango_read_reg(cpu, addr, insn.rn);
          uint32_t carry_in = (cpu->cpsr & MANGO_CPSR_C) ? 1u : 0u;
          uint32_t result = lhs + rhs + carry_in;
          cpu->r[insn.rd] = result;
          if (insn.sets_flags) {
            mango_set_nzcv(cpu, mango_flags_for_adc(lhs, rhs, carry_in, result));
          }
          break;
        }

        case MANGO_OP_RSC: {
          uint32_t op2 = mango_read_operand2(cpu, addr, &insn);
          uint32_t not_rn = ~mango_read_reg(cpu, addr, insn.rn); /* B-A-1+C == B+~A+C */
          uint32_t carry_in = (cpu->cpsr & MANGO_CPSR_C) ? 1u : 0u;
          uint32_t result = op2 + not_rn + carry_in;
          cpu->r[insn.rd] = result;
          if (insn.sets_flags) {
            mango_set_nzcv(cpu, mango_flags_for_adc(op2, not_rn, carry_in, result));
          }
          break;
        }

        case MANGO_OP_CMP: {
          uint32_t rhs = mango_read_operand2(cpu, addr, &insn);
          uint32_t lhs = mango_read_reg(cpu, addr, insn.rn);
          mango_set_nzcv(cpu, mango_flags_for_sub(lhs, rhs, lhs - rhs));
          break;
        }

        case MANGO_OP_MUL: {
          uint32_t result = mango_read_reg(cpu, addr, insn.rm) * mango_read_reg(cpu, addr, insn.rs);
          cpu->r[insn.rd] = result;
          if (insn.sets_flags) {
            /* MULS: C,V left as-is, same convention as the other logical-flag ops. */
            mango_set_nzcv(cpu, mango_flags_for_logical(cpu, result));
          }
          break;
        }

        case MANGO_OP_SVC:
          return 1; /* cpu->r[PC] == addr still, caller thunks r7/r0-r6 and resumes, see interp.h */

        case MANGO_OP_B:
          next_addr = addr + 8u + insn.imm; /* PC reads as addr+8 */
          break;

        case MANGO_OP_BL:
          cpu->r[MANGO_REG_LR] = addr + 4u;
          next_addr = addr + 8u + insn.imm;
          break;

        case MANGO_OP_BX:
          next_addr = cpu->r[insn.rm];
          break;

        case MANGO_OP_LDR:
        case MANGO_OP_STR: {
          if (insn.rd == MANGO_REG_PC) {
            return -1; /* indirect branch via LDR PC, not supported yet */
          }
          uint32_t base = mango_read_reg(cpu, addr, insn.rn);
          uint32_t eaddr = insn.u ? base + insn.imm : base - insn.imm;

          if (insn.b) {
            if (mango_check_byte_access(mem, eaddr) != 0) {
              return -1;
            }
            if (insn.op == MANGO_OP_LDR) {
              cpu->r[insn.rd] = mem->bytes[eaddr]; /* zero-extended */
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

  return -1; /* step limit hit */
}
