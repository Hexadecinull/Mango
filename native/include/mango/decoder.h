#ifndef MANGO_DECODER_H_
#define MANGO_DECODER_H_

#include <stdint.h>

/* Phase-1 subset of A32, see native/README.md for exact coverage. */

typedef enum MangoOp {
  MANGO_OP_UNKNOWN = 0,
  MANGO_OP_MOV,
  MANGO_OP_MVN,
  MANGO_OP_ADD,
  MANGO_OP_ADC,
  MANGO_OP_SUB,
  MANGO_OP_SBC,
  MANGO_OP_RSB,
  MANGO_OP_RSC,
  MANGO_OP_CMP,
  MANGO_OP_CMN,
  MANGO_OP_AND,
  MANGO_OP_EOR,
  MANGO_OP_ORR,
  MANGO_OP_BIC,
  MANGO_OP_TST,
  MANGO_OP_TEQ,
  MANGO_OP_MUL,
  MANGO_OP_SVC,
  MANGO_OP_B,
  MANGO_OP_BL,
  MANGO_OP_BX,
  MANGO_OP_LDR,
  MANGO_OP_STR,
} MangoOp;

typedef struct MangoInsn {
  MangoOp op;
  uint32_t cond; /* checked against NZCV by the interpreter, not here */
  uint32_t rd;
  uint32_t rn;
  uint32_t rm;           /* register form of operand2, or MUL's Rm */
  uint32_t rs;           /* MUL only: the other source register */
  uint32_t imm;          /* immediate operand2, branch offset, or LDR/STR offset */
  uint32_t shift_type;   /* 0=LSL,1=LSR,2=ASR,3=ROR, register operand2 only */
  uint32_t shift_amount; /* 0-31, register operand2 only */
  int is_imm;            /* 1 if operand2 is the immediate form */
  int sets_flags;        /* the S bit */
  int u;                 /* LDR/STR: 1 = add imm to base, 0 = subtract */
  int b;                 /* LDR/STR: 1 = byte access, 0 = word */
} MangoInsn;

/* 0 and fills *out on success, -1 for anything outside native/README.md's
 * documented subset. */
int mango_decode(uint32_t word, MangoInsn* out);

#endif /* MANGO_DECODER_H_ */
