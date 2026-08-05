#ifndef MANGO_CPU_H_
#define MANGO_CPU_H_

#include <stdint.h>

/* AArch32 register file, r13-r15 addressable as plain GPRs too. */
typedef struct MangoCpu {
  uint32_t r[16];
  uint32_t cpsr; /* NZCV in bits 31-28 */
} MangoCpu;

#define MANGO_REG_SP 13
#define MANGO_REG_LR 14
#define MANGO_REG_PC 15

#define MANGO_CPSR_N (1u << 31)
#define MANGO_CPSR_Z (1u << 30)
#define MANGO_CPSR_C (1u << 29)
#define MANGO_CPSR_V (1u << 28)

#endif /* MANGO_CPU_H_ */
