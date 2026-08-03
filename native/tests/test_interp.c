#include <stdio.h>
#include <string.h>

#include "mango/cpu.h"
#include "mango/interp.h"

/* Hand-encoded A32, cond=AL throughout. See native/README.md before
 * trusting hex values blindly, worth re-deriving by hand when you read
 * this; that's caught real bugs during development more than once. */

/* Copies words into mem little-endian, starting at byte 0, and zeroes
 * the rest. A whole test's address space in one buffer, code and data
 * together, same as the real thing. */
static void load_words(uint8_t* mem, uint32_t mem_size, const uint32_t* words, uint32_t n) {
  memset(mem, 0, mem_size);
  for (uint32_t i = 0; i < n; i++) {
    uint32_t w = words[i];
    mem[i * 4 + 0] = (uint8_t)(w & 0xFF);
    mem[i * 4 + 1] = (uint8_t)((w >> 8) & 0xFF);
    mem[i * 4 + 2] = (uint8_t)((w >> 16) & 0xFF);
    mem[i * 4 + 3] = (uint8_t)((w >> 24) & 0xFF);
  }
}

/* Local to this test file; interp.c's own version is static, not exported. */
static uint32_t bytes_to_u32_le(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int test_mov_add_bx(void) {
  static const uint32_t kProgram[] = {
      0xE3A00002u, /* mov r0, #2 */
      0xE2800003u, /* add r0, r0, #3 */
      0xE12FFF1Eu, /* bx lr */
  };

  uint8_t mem_buf[64];
  load_words(mem_buf, sizeof(mem_buf), kProgram, 3);
  MangoMemory mem = {mem_buf, sizeof(mem_buf)};

  MangoCpu cpu;
  for (int i = 0; i < 16; i++) {
    cpu.r[i] = 0;
  }
  cpu.cpsr = 0;
  cpu.r[MANGO_REG_LR] = 0xDEADBEEFu;

  int rc = mango_interp_run(&cpu, &mem, 100);
  if (rc != 0) {
    fprintf(stderr, "FAIL(mov_add_bx): mango_interp_run returned %d\n", rc);
    return 1;
  }
  if (cpu.r[0] != 5) {
    fprintf(stderr, "FAIL(mov_add_bx): expected r0 == 5, got %u\n", cpu.r[0]);
    return 1;
  }
  if (cpu.r[MANGO_REG_PC] != 0xDEADBEEFu) {
    fprintf(stderr, "FAIL(mov_add_bx): expected pc == sentinel LR, got 0x%08x\n",
            cpu.r[MANGO_REG_PC]);
    return 1;
  }
  printf("ok: mov + add + bx (r0 = %u)\n", cpu.r[0]);
  return 0;
}

static int test_branch_and_flags(void) {
  /* mov r0, #10 ; b skip ; mov r0, #99 (skipped) ; skip: sub r0, r0, #4 ;
   * cmp r0, #6 ; bx lr */
  static const uint32_t kProgram[] = {
      0xE3A0000Au, /* mov r0, #10 */
      0xEA000000u, /* b skip (skip = this instr's addr + 8) */
      0xE3A00063u, /* mov r0, #99, must never execute */
      0xE2400004u, /* skip: sub r0, r0, #4 */
      0xE3500006u, /* cmp r0, #6 */
      0xE12FFF1Eu, /* bx lr */
  };

  uint8_t mem_buf[64];
  load_words(mem_buf, sizeof(mem_buf), kProgram, 6);
  MangoMemory mem = {mem_buf, sizeof(mem_buf)};

  MangoCpu cpu;
  for (int i = 0; i < 16; i++) {
    cpu.r[i] = 0;
  }
  cpu.cpsr = 0;
  cpu.r[MANGO_REG_LR] = 0xCAFEF00Du;

  int rc = mango_interp_run(&cpu, &mem, 100);
  if (rc != 0) {
    fprintf(stderr, "FAIL(branch_and_flags): mango_interp_run returned %d\n", rc);
    return 1;
  }
  if (cpu.r[0] != 6) {
    fprintf(stderr,
            "FAIL(branch_and_flags): expected r0 == 6 (branch must have "
            "skipped mov r0,#99), got %u\n",
            cpu.r[0]);
    return 1;
  }
  if (!(cpu.cpsr & MANGO_CPSR_Z)) {
    fprintf(stderr, "FAIL(branch_and_flags): expected Z flag set after cmp r0,#6\n");
    return 1;
  }
  printf("ok: b + sub + cmp (r0 = %u, Z set)\n", cpu.r[0]);
  return 0;
}

static int test_load_store_roundtrip(void) {
  /* mov r0, #100 ; mov r1, #64 ; str r0, [r1] ; mov r0, #0 ;
   * ldr r0, [r1] ; bx lr
   * Program is 6 words (24 bytes); r1 points at byte 64, comfortably
   * past the program itself, so code and the word we store/load don't
   * overlap. */
  static const uint32_t kProgram[] = {
      0xE3A00064u, /* mov r0, #100 */
      0xE3A01040u, /* mov r1, #64 */
      0xE5810000u, /* str r0, [r1] */
      0xE3A00000u, /* mov r0, #0 */
      0xE5910000u, /* ldr r0, [r1] */
      0xE12FFF1Eu, /* bx lr */
  };

  uint8_t mem_buf[128];
  load_words(mem_buf, sizeof(mem_buf), kProgram, 6);
  MangoMemory mem = {mem_buf, sizeof(mem_buf)};

  MangoCpu cpu;
  for (int i = 0; i < 16; i++) {
    cpu.r[i] = 0;
  }
  cpu.cpsr = 0;
  cpu.r[MANGO_REG_LR] = 0x1234u;

  int rc = mango_interp_run(&cpu, &mem, 100);
  if (rc != 0) {
    fprintf(stderr, "FAIL(load_store_roundtrip): mango_interp_run returned %d\n", rc);
    return 1;
  }
  if (cpu.r[0] != 100) {
    fprintf(stderr,
            "FAIL(load_store_roundtrip): expected r0 == 100 after store+load "
            "round trip, got %u\n",
            cpu.r[0]);
    return 1;
  }
  /* Byte-check the store actually landed where expected, not just that
   * the load happened to return the right value some other way. */
  uint32_t stored = bytes_to_u32_le(mem_buf + 64);
  if (stored != 100) {
    fprintf(stderr, "FAIL(load_store_roundtrip): expected mem[64..67] == 100, got %u\n", stored);
    return 1;
  }
  printf("ok: str + ldr round trip (r0 = %u)\n", cpu.r[0]);
  return 0;
}

static int test_pc_relative_add(void) {
  /* add r0, pc, #4 ; bx lr
   * At address 0, PC reads as 0 + 8 = 8 per real hardware semantics, so
   * r0 should end up 8 + 4 = 12. This is the same addressing real
   * compiled code uses for literal-pool constants. */
  static const uint32_t kProgram[] = {
      0xE28F0004u, /* add r0, pc, #4 */
      0xE12FFF1Eu, /* bx lr */
  };

  uint8_t mem_buf[32];
  load_words(mem_buf, sizeof(mem_buf), kProgram, 2);
  MangoMemory mem = {mem_buf, sizeof(mem_buf)};

  MangoCpu cpu;
  for (int i = 0; i < 16; i++) {
    cpu.r[i] = 0;
  }
  cpu.cpsr = 0;
  cpu.r[MANGO_REG_LR] = 0x5678u;

  int rc = mango_interp_run(&cpu, &mem, 100);
  if (rc != 0) {
    fprintf(stderr, "FAIL(pc_relative_add): mango_interp_run returned %d\n", rc);
    return 1;
  }
  if (cpu.r[0] != 12) {
    fprintf(stderr, "FAIL(pc_relative_add): expected r0 == 12, got %u\n", cpu.r[0]);
    return 1;
  }
  printf("ok: pc-relative add (r0 = %u)\n", cpu.r[0]);
  return 0;
}

static int test_load_out_of_bounds_rejected(void) {
  /* mov r1, #200 (way past the 32-byte buffer) ; ldr r0, [r1] ; bx lr
   * Must fail cleanly, not read past the buffer. Real bug class this
   * guards against: an ASan build would catch a broken bounds check
   * here immediately, that's the point of running this under
   * -fsanitize=address in CI rather than just eyeballing the code. */
  static const uint32_t kProgram[] = {
      0xE3A010C8u, /* mov r1, #200 */
      0xE5910000u, /* ldr r0, [r1] */
      0xE12FFF1Eu, /* bx lr */
  };

  uint8_t mem_buf[32];
  load_words(mem_buf, sizeof(mem_buf), kProgram, 3);
  MangoMemory mem = {mem_buf, sizeof(mem_buf)};

  MangoCpu cpu;
  for (int i = 0; i < 16; i++) {
    cpu.r[i] = 0;
  }
  cpu.cpsr = 0;

  int rc = mango_interp_run(&cpu, &mem, 100);
  if (rc == 0) {
    fprintf(stderr, "FAIL(load_out_of_bounds_rejected): expected a failure, got success\n");
    return 1;
  }
  printf("ok: out-of-bounds ldr correctly rejected\n");
  return 0;
}

static int test_load_misaligned_rejected(void) {
  /* mov r1, #1 ; ldr r0, [r1] ; bx lr -- address 1 is never word-aligned. */
  static const uint32_t kProgram[] = {
      0xE3A01001u, /* mov r1, #1 */
      0xE5910000u, /* ldr r0, [r1] */
      0xE12FFF1Eu, /* bx lr */
  };

  uint8_t mem_buf[32];
  load_words(mem_buf, sizeof(mem_buf), kProgram, 3);
  MangoMemory mem = {mem_buf, sizeof(mem_buf)};

  MangoCpu cpu;
  for (int i = 0; i < 16; i++) {
    cpu.r[i] = 0;
  }
  cpu.cpsr = 0;

  int rc = mango_interp_run(&cpu, &mem, 100);
  if (rc == 0) {
    fprintf(stderr, "FAIL(load_misaligned_rejected): expected a failure, got success\n");
    return 1;
  }
  printf("ok: misaligned ldr correctly rejected\n");
  return 0;
}

static int test_load_store_byte_roundtrip(void) {
  /* mov r0, #171 ; mov r1, #64 ; strb r0, [r1] ; mov r0, #0 ;
   * ldrb r0, [r1] ; bx lr
   * 171 (0xAB) has the high bit of the byte set: if LDRB ever
   * accidentally sign-extended instead of zero-extending, this would
   * come back as 0xFFFFFFAB instead of 0xAB and the test would catch
   * it. */
  static const uint32_t kProgram[] = {
      0xE3A000ABu, /* mov r0, #171 */
      0xE3A01040u, /* mov r1, #64 */
      0xE5C10000u, /* strb r0, [r1] */
      0xE3A00000u, /* mov r0, #0 */
      0xE5D10000u, /* ldrb r0, [r1] */
      0xE12FFF1Eu, /* bx lr */
  };

  uint8_t mem_buf[128];
  load_words(mem_buf, sizeof(mem_buf), kProgram, 6);
  MangoMemory mem = {mem_buf, sizeof(mem_buf)};

  MangoCpu cpu;
  for (int i = 0; i < 16; i++) {
    cpu.r[i] = 0;
  }
  cpu.cpsr = 0;
  cpu.r[MANGO_REG_LR] = 0x9999u;

  int rc = mango_interp_run(&cpu, &mem, 100);
  if (rc != 0) {
    fprintf(stderr, "FAIL(load_store_byte_roundtrip): mango_interp_run returned %d\n", rc);
    return 1;
  }
  if (cpu.r[0] != 171) {
    fprintf(stderr, "FAIL(load_store_byte_roundtrip): expected r0 == 171, got %u (0x%08x)\n",
            cpu.r[0], cpu.r[0]);
    return 1;
  }
  /* Byte-check the neighboring bytes were left alone (STRB must not
   * touch more than one byte). */
  if (mem_buf[65] != 0 || mem_buf[66] != 0 || mem_buf[67] != 0) {
    fprintf(stderr, "FAIL(load_store_byte_roundtrip): strb touched neighboring bytes\n");
    return 1;
  }
  printf("ok: strb + ldrb round trip, zero-extended (r0 = %u)\n", cpu.r[0]);
  return 0;
}

static int test_conditional_branch_taken(void) {
  /* mov r0, #5 ; cmp r0, #5 ; beq target ; mov r0, #99 (must be skipped) ;
   * target: mov r1, #1 ; bx lr
   * r0 == r0, so Z is set and the branch must be taken. */
  static const uint32_t kProgram[] = {
      0xE3A00005u, /* mov r0, #5 */
      0xE3500005u, /* cmp r0, #5 */
      0x0A000000u, /* beq target (target = this instr's addr + 8) */
      0xE3A00063u, /* mov r0, #99, must never execute */
      0xE3A01001u, /* target: mov r1, #1 */
      0xE12FFF1Eu, /* bx lr */
  };

  uint8_t mem_buf[64];
  load_words(mem_buf, sizeof(mem_buf), kProgram, 6);
  MangoMemory mem = {mem_buf, sizeof(mem_buf)};

  MangoCpu cpu;
  for (int i = 0; i < 16; i++) {
    cpu.r[i] = 0;
  }
  cpu.cpsr = 0;
  cpu.r[MANGO_REG_LR] = 0x1111u;

  int rc = mango_interp_run(&cpu, &mem, 100);
  if (rc != 0) {
    fprintf(stderr, "FAIL(conditional_branch_taken): mango_interp_run returned %d\n", rc);
    return 1;
  }
  if (cpu.r[0] != 5) {
    fprintf(stderr,
            "FAIL(conditional_branch_taken): expected r0 == 5 (beq must have "
            "skipped mov r0,#99), got %u\n",
            cpu.r[0]);
    return 1;
  }
  if (cpu.r[1] != 1) {
    fprintf(stderr,
            "FAIL(conditional_branch_taken): expected r1 == 1 (target must "
            "have run), got %u\n",
            cpu.r[1]);
    return 1;
  }
  printf("ok: beq taken (r0 = %u, r1 = %u)\n", cpu.r[0], cpu.r[1]);
  return 0;
}

static int test_signed_vs_unsigned_condition_flags(void) {
  /* mov r0, #0 ; sub r0, r0, #1 (r0 = 0xFFFFFFFF, i.e. -1 signed) ;
   * cmp r0, #1 ; movlt r1, #1 (signed: -1 < 1, should execute) ;
   * movge r2, #1 (signed: NOT -1 >= 1, must not execute) ; bx lr
   *
   * The point of this test: 0xFFFFFFFF is simultaneously "less than 1"
   * as a signed number and "greater than 1" as an unsigned one. Getting
   * this right depends on C and V being computed correctly in CMP, not
   * just N and Z, this is exactly the kind of case that would silently
   * pass with N/Z alone but fail here. */
  static const uint32_t kProgram[] = {
      0xE3A00000u, /* mov r0, #0 */
      0xE2400001u, /* sub r0, r0, #1 */
      0xE3500001u, /* cmp r0, #1 */
      0xB3A01001u, /* movlt r1, #1 */
      0xA3A02001u, /* movge r2, #1 */
      0xE12FFF1Eu, /* bx lr */
  };

  uint8_t mem_buf[64];
  load_words(mem_buf, sizeof(mem_buf), kProgram, 6);
  MangoMemory mem = {mem_buf, sizeof(mem_buf)};

  MangoCpu cpu;
  for (int i = 0; i < 16; i++) {
    cpu.r[i] = 0;
  }
  cpu.cpsr = 0;
  cpu.r[MANGO_REG_LR] = 0x2222u;

  int rc = mango_interp_run(&cpu, &mem, 100);
  if (rc != 0) {
    fprintf(stderr, "FAIL(signed_vs_unsigned_condition_flags): mango_interp_run returned %d\n", rc);
    return 1;
  }
  if (cpu.r[0] != 0xFFFFFFFFu) {
    fprintf(stderr, "FAIL(signed_vs_unsigned_condition_flags): expected r0 == -1, got %u\n",
            cpu.r[0]);
    return 1;
  }
  if (cpu.r[1] != 1) {
    fprintf(stderr,
            "FAIL(signed_vs_unsigned_condition_flags): expected r1 == 1 "
            "(movlt should have executed, -1 < 1), got %u\n",
            cpu.r[1]);
    return 1;
  }
  if (cpu.r[2] != 0) {
    fprintf(stderr,
            "FAIL(signed_vs_unsigned_condition_flags): expected r2 == 0 "
            "(movge should NOT have executed, -1 is not >= 1), got %u\n",
            cpu.r[2]);
    return 1;
  }
  if ((cpu.cpsr & MANGO_CPSR_C) == 0) {
    fprintf(stderr,
            "FAIL(signed_vs_unsigned_condition_flags): expected C set "
            "(0xFFFFFFFF >= 1 unsigned)\n");
    return 1;
  }
  if ((cpu.cpsr & MANGO_CPSR_V) != 0) {
    fprintf(stderr,
            "FAIL(signed_vs_unsigned_condition_flags): expected V clear "
            "(-1 - 1 doesn't signed-overflow)\n");
    return 1;
  }
  printf("ok: signed vs unsigned flags (r1 = %u, r2 = %u)\n", cpu.r[1], cpu.r[2]);
  return 0;
}

static int test_shifted_operand2(void) {
  /* mov r0, #16 ; mov r1, r0, LSL #2 ; mov r2, r0, LSR #2 ; mov r0, #0 ;
   * sub r0, r0, #16 (r0 = -16 = 0xFFFFFFF0) ; mov r3, r0, ASR #2 ;
   * mov r4, r0, ROR #4 ; bx lr
   * ASR specifically needs a negative input to actually distinguish
   * itself from LSR (they're identical for positive values), that's why
   * r0 gets reused as -16 partway through instead of adding a 7th
   * register. */
  static const uint32_t kProgram[] = {
      0xE3A00010u, /* mov r0, #16 */
      0xE1A01100u, /* mov r1, r0, LSL #2 */
      0xE1A02120u, /* mov r2, r0, LSR #2 */
      0xE3A00000u, /* mov r0, #0 */
      0xE2400010u, /* sub r0, r0, #16 */
      0xE1A03140u, /* mov r3, r0, ASR #2 */
      0xE1A04260u, /* mov r4, r0, ROR #4 */
      0xE12FFF1Eu, /* bx lr */
  };

  uint8_t mem_buf[64];
  load_words(mem_buf, sizeof(mem_buf), kProgram, 8);
  MangoMemory mem = {mem_buf, sizeof(mem_buf)};

  MangoCpu cpu;
  for (int i = 0; i < 16; i++) {
    cpu.r[i] = 0;
  }
  cpu.cpsr = 0;
  cpu.r[MANGO_REG_LR] = 0x3333u;

  int rc = mango_interp_run(&cpu, &mem, 100);
  if (rc != 0) {
    fprintf(stderr, "FAIL(shifted_operand2): mango_interp_run returned %d\n", rc);
    return 1;
  }
  struct {
    uint32_t reg;
    uint32_t got;
    uint32_t want;
    const char* label;
  } checks[] = {
      {1, cpu.r[1], 64, "LSL #2 of 16"},
      {2, cpu.r[2], 4, "LSR #2 of 16"},
      {3, cpu.r[3], 0xFFFFFFFCu, "ASR #2 of -16"},
      {4, cpu.r[4], 0x0FFFFFFFu, "ROR #4 of -16"},
  };
  for (size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); i++) {
    if (checks[i].got != checks[i].want) {
      fprintf(stderr, "FAIL(shifted_operand2): %s: expected 0x%08x, got 0x%08x\n",
              checks[i].label, checks[i].want, checks[i].got);
      return 1;
    }
  }
  printf("ok: shifted operand2, LSL/LSR/ASR/ROR all correct\n");
  return 0;
}

static int test_adds_signed_overflow_without_carry(void) {
  /* mov r0, #1 ; mov r0, r0, LSL #30 (r0 = 0x40000000) ;
   * adds r0, r0, r0 (r0 = 0x80000000) ; bx lr
   *
   * The point: 0x40000000 + 0x40000000 = 0x80000000 fits fine in 32
   * unsigned bits (no carry out, C should be 0), but as signed 32-bit
   * numbers it's a textbook overflow, two positives summing to
   * something that looks negative (V should be 1). Getting V right
   * without it just tracking C is exactly what this checks. */
  static const uint32_t kProgram[] = {
      0xE3A00001u, /* mov r0, #1 */
      0xE1A00F00u, /* mov r0, r0, LSL #30 */
      0xE0900000u, /* adds r0, r0, r0 */
      0xE12FFF1Eu, /* bx lr */
  };

  uint8_t mem_buf[32];
  load_words(mem_buf, sizeof(mem_buf), kProgram, 4);
  MangoMemory mem = {mem_buf, sizeof(mem_buf)};

  MangoCpu cpu;
  for (int i = 0; i < 16; i++) {
    cpu.r[i] = 0;
  }
  cpu.cpsr = 0;
  cpu.r[MANGO_REG_LR] = 0x4444u;

  int rc = mango_interp_run(&cpu, &mem, 100);
  if (rc != 0) {
    fprintf(stderr, "FAIL(adds_signed_overflow_without_carry): mango_interp_run returned %d\n", rc);
    return 1;
  }
  if (cpu.r[0] != 0x80000000u) {
    fprintf(stderr, "FAIL(adds_overflow): expected r0 == 0x80000000, got 0x%08x\n", cpu.r[0]);
    return 1;
  }
  if ((cpu.cpsr & MANGO_CPSR_C) != 0) {
    fprintf(stderr, "FAIL(adds_overflow): expected C clear (no unsigned carry)\n");
    return 1;
  }
  if ((cpu.cpsr & MANGO_CPSR_V) == 0) {
    fprintf(stderr, "FAIL(adds_overflow): expected V set (signed overflow)\n");
    return 1;
  }
  printf("ok: adds signed overflow without carry (r0 = 0x%08x)\n", cpu.r[0]);
  return 0;
}

static int test_subs_borrow_no_overflow(void) {
  /* mov r0, #5 ; subs r0, r0, #10 (r0 = -5) ; bx lr
   * 5 - 10 needs a borrow (C should clear), but -5 is a completely
   * ordinary in-range signed result, not an overflow (V should stay
   * clear). Checks C and V are computed independently, not one implying
   * the other. */
  static const uint32_t kProgram[] = {
      0xE3A00005u, /* mov r0, #5 */
      0xE250000Au, /* subs r0, r0, #10 */
      0xE12FFF1Eu, /* bx lr */
  };

  uint8_t mem_buf[32];
  load_words(mem_buf, sizeof(mem_buf), kProgram, 3);
  MangoMemory mem = {mem_buf, sizeof(mem_buf)};

  MangoCpu cpu;
  for (int i = 0; i < 16; i++) {
    cpu.r[i] = 0;
  }
  cpu.cpsr = 0;
  cpu.r[MANGO_REG_LR] = 0x5555u;

  int rc = mango_interp_run(&cpu, &mem, 100);
  if (rc != 0) {
    fprintf(stderr, "FAIL(subs_borrow_no_overflow): mango_interp_run returned %d\n", rc);
    return 1;
  }
  if (cpu.r[0] != 0xFFFFFFFBu) {
    fprintf(stderr, "FAIL(subs_borrow_no_overflow): expected r0 == -5, got 0x%08x\n", cpu.r[0]);
    return 1;
  }
  if ((cpu.cpsr & MANGO_CPSR_C) != 0) {
    fprintf(stderr, "FAIL(subs_borrow_no_overflow): expected C clear (5 < 10, borrow occurred)\n");
    return 1;
  }
  if ((cpu.cpsr & MANGO_CPSR_V) != 0) {
    fprintf(stderr, "FAIL(subs_borrow_no_overflow): expected V clear (-5 doesn't overflow)\n");
    return 1;
  }
  printf("ok: subs borrow without overflow (r0 = 0x%08x)\n", cpu.r[0]);
  return 0;
}

int main(void) {
  int failures = 0;
  failures += test_mov_add_bx();
  failures += test_branch_and_flags();
  failures += test_load_store_roundtrip();
  failures += test_pc_relative_add();
  failures += test_load_out_of_bounds_rejected();
  failures += test_load_misaligned_rejected();
  failures += test_load_store_byte_roundtrip();
  failures += test_conditional_branch_taken();
  failures += test_signed_vs_unsigned_condition_flags();
  failures += test_shifted_operand2();
  failures += test_adds_signed_overflow_without_carry();
  failures += test_subs_borrow_no_overflow();

  if (failures != 0) {
    fprintf(stderr, "%d test(s) failed\n", failures);
    return 1;
  }
  printf("all tests passed\n");
  return 0;
}
