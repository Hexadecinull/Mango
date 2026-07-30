#ifndef MANGO_DECODER_H_
#define MANGO_DECODER_H_

#include <stdint.h>

/*
 * Phase-1 proof of concept only: decodes a handful of unconditional-flag,
 * AL-condition A32 instructions, not Thumb-2, not real-world code. See
 * docs/ARCHITECTURE.md, "Realistic roadmap", phase 1.
 *
 * Encodings below are per the ARMv7-A architecture reference manual
 * (DDI 0406C), typed from memory. Double check bit positions against the
 * actual manual before trusting this for anything beyond the test in
 * native/tests/, this is exactly the kind of thing worth a second pair
 * of eyes on.
 */

typedef enum MangoOp {
  MANGO_OP_UNKNOWN = 0,
  MANGO_OP_MOV,
  MANGO_OP_ADD,
  MANGO_OP_SUB,
  MANGO_OP_CMP,
  MANGO_OP_B,
  MANGO_OP_BX,
} MangoOp;

typedef struct MangoInsn {
  MangoOp op;
  uint32_t cond;   /* bits 31-28, only 0xE (AL) is handled right now */
  uint32_t rd;
  uint32_t rn;
  uint32_t rm;      /* register form of operand2 */
  uint32_t imm;      /* immediate form of operand2, or branch offset */
  int is_imm;        /* 1 if operand2 is the immediate form */
  int sets_flags;     /* the S bit */
} MangoInsn;

/* Decodes one 32-bit A32 word. Returns 0 and fills *out on success,
 * -1 for anything not in the small subset above. */
int mango_decode(uint32_t word, MangoInsn* out);

#endif /* MANGO_DECODER_H_ */
