#include "mango/interp.h"

#include "mango/decoder.h"

int mango_interp_run(MangoCpu* cpu, const uint32_t* code, uint32_t code_words, uint32_t max_steps) {
  uint32_t addr = cpu->r[MANGO_REG_PC];

  for (uint32_t step = 0; step < max_steps; step++) {
    if (addr >= code_words * 4u || (addr % 4u) != 0) {
      return -1;
    }

    MangoInsn insn;
    if (mango_decode(code[addr / 4u], &insn) != 0) {
      return -1;
    }

    uint32_t next_addr = addr + 4u;

    switch (insn.op) {
      case MANGO_OP_MOV:
        cpu->r[insn.rd] = insn.is_imm ? insn.imm : cpu->r[insn.rm];
        break;

      case MANGO_OP_ADD:
        cpu->r[insn.rd] = cpu->r[insn.rn] + (insn.is_imm ? insn.imm : cpu->r[insn.rm]);
        break;

      case MANGO_OP_SUB:
        cpu->r[insn.rd] = cpu->r[insn.rn] - (insn.is_imm ? insn.imm : cpu->r[insn.rm]);
        break;

      case MANGO_OP_CMP: {
        uint32_t rhs = insn.is_imm ? insn.imm : cpu->r[insn.rm];
        uint32_t result = cpu->r[insn.rn] - rhs;
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

      default:
        return -1;
    }

    addr = next_addr;
    cpu->r[MANGO_REG_PC] = addr;
  }

  return -1; /* step limit hit, most likely an infinite loop in the guest */
}
