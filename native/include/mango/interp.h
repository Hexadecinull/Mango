#ifndef MANGO_INTERP_H_
#define MANGO_INTERP_H_

#include <stdint.h>

#include "mango/cpu.h"

/*
 * Interprets starting at cpu->r[PC] until a BX (treated as "return" for
 * this proof of concept) or max_steps is hit. code/code_words describe a
 * flat array of A32 words, addressed as if code[0] were address 0.
 * Returns 0 on a clean BX, -1 on anything else (unknown instruction,
 * out-of-range address, step limit).
 */
int mango_interp_run(MangoCpu* cpu, const uint32_t* code, uint32_t code_words, uint32_t max_steps);

#endif /* MANGO_INTERP_H_ */
