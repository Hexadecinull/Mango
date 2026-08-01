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

    switch (insn.op) {
      case MANGO_OP_MOV:
        cpu->r[insn.rd] = insn.is_imm ? insn.imm : mango_read_reg(cpu, addr, insn.rm);
        break;

      case MANGO_OP_ADD: {
        uint32_t rhs = insn.is_imm ? insn.imm : mango_read_reg(cpu, addr, insn.rm);
        cpu->r[insn.rd] = mango_read_reg(cpu, addr, insn.rn) + rhs;
        break;
      }

      case MANGO_OP_SUB: {
        uint32_t rhs = insn.is_imm ? insn.imm : mango_read_reg(cpu, addr, insn.rm);
        cpu->r[insn.rd] = mango_read_reg(cpu, addr, insn.rn) - rhs;
        break;
      }

      case MANGO_OP_CMP: {
        uint32_t rhs = insn.is_imm ? insn.imm : mango_read_reg(cpu, addr, insn.rm);
        uint32_t result = mango_read_reg(cpu, addr, insn.rn) - rhs;
        cpu->cpsr &= ~(MANGO_CPSR_Z | MANGO_CPSR_N);
        cpu->cpsr |= (result == 0) ? MANGO_CPSR_Z : 0;
        cpu->cpsr |= (result & 0x80000000u) ? MANGO_CPSR_N : 0;
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

    addr = next_addr;
    cpu->r[MANGO_REG_PC] = addr;
  }

  return -1; /* step limit hit, most likely an infinite loop in the guest */
}
