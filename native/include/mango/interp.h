#ifndef MANGO_INTERP_H_
#define MANGO_INTERP_H_

#include <stdint.h>

#include "mango/cpu.h"

/* Flat byte-addressable guest memory; code and data share it. Caller-owned. */
typedef struct MangoMemory {
  uint8_t* bytes;
  uint32_t size;
} MangoMemory;

/* Runs from cpu->r[PC] until PC == stop_addr or max_steps is hit. BX is a
 * normal jump, not an exit, so nested calls (BL then callee's BX LR) work;
 * callers pick a stop_addr outside mem and preload LR with it to detect
 * top-level return. 0 on reaching stop_addr, -1 otherwise. */
int mango_interp_run(MangoCpu* cpu, MangoMemory* mem, uint32_t stop_addr, uint32_t max_steps);

#endif /* MANGO_INTERP_H_ */
