#ifndef MANGO_INTERP_H_
#define MANGO_INTERP_H_

#include <stdint.h>

#include "mango/cpu.h"

/* Flat byte-addressable guest memory; code and data share it. Caller-owned. */
typedef struct MangoMemory {
  uint8_t* bytes;
  uint32_t size;
} MangoMemory;

/* Runs from cpu->r[PC] until PC == stop_addr, max_steps is hit, or an SVC
 * is reached. Callers pick a stop_addr outside mem and preload LR with it
 * to detect top-level return (BX is a normal jump, not an exit, so nested
 * calls, BL then callee's BX LR, work). Returns 0 on reaching stop_addr,
 * 1 on SVC (r7 = syscall number, r0-r6 = args, PC left pointing AT the
 * SVC, not past it; a caller that handles the syscall should set r0 to
 * the result, advance PC by 4 itself, and call this again to resume), -1
 * on anything else (bad decode, out-of-bounds/misaligned access, or
 * max_steps exceeded). */
int mango_interp_run(MangoCpu* cpu, MangoMemory* mem, uint32_t stop_addr, uint32_t max_steps);

#endif /* MANGO_INTERP_H_ */
