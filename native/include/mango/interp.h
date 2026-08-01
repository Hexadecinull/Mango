#ifndef MANGO_INTERP_H_
#define MANGO_INTERP_H_

#include <stdint.h>

#include "mango/cpu.h"

/*
 * A flat, byte-addressable guest address space: code and data share it,
 * same as real memory. bytes/size describe a buffer the caller owns;
 * mango_interp_run only reads it (for fetch) and reads/writes it (for
 * LDR/STR), never resizes or frees it.
 */
typedef struct MangoMemory {
  uint8_t* bytes;
  uint32_t size;
} MangoMemory;

/*
 * Interprets starting at cpu->r[PC] until a BX (treated as "return" for
 * this proof of concept) or max_steps is hit. Every fetch and every
 * LDR/STR is bounds- and alignment-checked against mem; anything out of
 * range fails the whole run rather than reading/writing out of bounds,
 * see docs/SECURITY.md on why that matters here specifically.
 * Returns 0 on a clean BX, -1 on anything else (unknown instruction,
 * out-of-range or misaligned access, step limit).
 */
int mango_interp_run(MangoCpu* cpu, MangoMemory* mem, uint32_t max_steps);

#endif /* MANGO_INTERP_H_ */
