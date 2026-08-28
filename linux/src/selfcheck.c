/*
 * Standalone host demo: proves native/'s decoder+interpreter core builds
 * and runs as an ordinary Linux program, no Android/NDK/JNI in the loop.
 * Not a translator and not the real test suite (that's
 * native/tests/test_interp.c); see linux/README.md for what's still
 * missing before this becomes one.
 */
#include <stdio.h>
#include <string.h>

#include "mango/cpu.h"
#include "mango/interp.h"

/* Local copy, not exported by interp.c; native/tests/test_interp.c has
 * the same helper for the same reason (see that file's own comment). */
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

int main(void) {
  /* mov r0, #21 ; add r0, r0, #21 ; bx lr -- expect r0 == 42 on return. */
  static const uint32_t kProgram[] = {
      0xE3A00015u, /* mov r0, #21 */
      0xE2800015u, /* add r0, r0, #21 */
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
  cpu.r[MANGO_REG_LR] = 0xDEADBEEFu; /* sentinel stop_addr, outside mem */

  int rc = mango_interp_run(&cpu, &mem, 0xDEADBEEFu, 16);
  if (rc != 0 || cpu.r[0] != 42) {
    fprintf(stderr, "FAIL: rc=%d r0=%u (want rc=0 r0=42)\n", rc, cpu.r[0]);
    return 1;
  }

  printf("PASS: mango_core runs standalone on Linux, r0=%u\n", cpu.r[0]);
  return 0;
}
