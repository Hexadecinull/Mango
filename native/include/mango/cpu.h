#ifndef MANGO_CPU_H_
#define MANGO_CPU_H_

#include <stdint.h>

/* Guest (AArch32) register file. R13-R15 are conventionally SP/LR/PC but
 * are modeled as plain GPRs here too, since A32 code can address them
 * directly as Rn/Rd in most data-processing instructions. */
typedef struct MangoCpu {
  uint32_t r[16];
  uint32_t cpsr; /* only N, Z, C, V flags used for now, bits 31-28 */
} MangoCpu;

#define MANGO_REG_SP 13
#define MANGO_REG_LR 14
#define MANGO_REG_PC 15

#define MANGO_CPSR_N (1u << 31)
#define MANGO_CPSR_Z (1u << 30)
#define MANGO_CPSR_C (1u << 29)
#define MANGO_CPSR_V (1u << 28)

#endif /* MANGO_CPU_H_ */
