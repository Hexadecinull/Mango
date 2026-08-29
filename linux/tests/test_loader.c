#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mango/linux_loader.h"

/*
 * A hand-built, minimal ELF32 *executable* (not a shared object): one
 * PT_LOAD segment holding two real, already-verified A32 instructions
 * (mov r0, #42 ; bx lr), no section headers at all (mango_elf32_parse
 * doesn't require them). Independently verified with `readelf` during
 * development, see native/README.md and native/tests/test_elf32.c for
 * the same approach applied to a shared object instead of an executable.
 */
static const uint8_t kSynthExec[] = {
    0x7Fu, 0x45u, 0x4Cu, 0x46u, 0x01u, 0x01u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x28u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u,
    0x54u, 0x00u, 0x00u, 0x00u, 0x34u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x34u, 0x00u, 0x20u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x5Cu, 0x00u, 0x00u, 0x00u,
    0x5Cu, 0x00u, 0x00u, 0x00u, 0x05u, 0x00u, 0x00u, 0x00u, 0x00u, 0x10u, 0x00u, 0x00u,
    0x2Au, 0x00u, 0xA0u, 0xE3u, 0x1Eu, 0xFFu, 0x2Fu, 0xE1u,
};

static int test_loads_and_runs_a_real_binary(void) {
  MangoLinuxLoadResult result = mango_linux_load_and_run(kSynthExec, sizeof(kSynthExec));
  if (result.status != MANGO_LINUX_LOAD_OK) {
    fprintf(stderr, "FAIL(loads_and_runs_a_real_binary): status=%d, want OK\n", result.status);
    return 1;
  }
  if (result.r0 != 42) {
    fprintf(stderr, "FAIL(loads_and_runs_a_real_binary): r0=%u, want 42\n", result.r0);
    return 1;
  }
  printf("ok: loads a real ELF32 executable and runs it end to end (r0 = %u)\n", result.r0);
  return 0;
}

static int test_rejects_non_elf_input(void) {
  static const uint8_t kNotElf[] = {'n', 'o', 't', ' ', 'a', 'n', ' ', 'e', 'l', 'f'};
  MangoLinuxLoadResult result = mango_linux_load_and_run(kNotElf, sizeof(kNotElf));
  if (result.status != MANGO_LINUX_LOAD_BAD_ELF) {
    fprintf(stderr, "FAIL(rejects_non_elf_input): status=%d, want MANGO_LINUX_LOAD_BAD_ELF\n",
            result.status);
    return 1;
  }
  printf("ok: non-ELF input is rejected cleanly, not a crash\n");
  return 0;
}

static int test_reports_undecodable_instructions_without_crashing(void) {
  /* Same fixture, but with the second instruction (bx lr) corrupted into
   * all-zero bytes, which mango_core's decoder doesn't recognize as
   * anything: exactly the SVC-shaped situation this status exists for. */
  uint8_t corrupted[sizeof(kSynthExec)];
  for (size_t i = 0; i < sizeof(kSynthExec); i++) {
    corrupted[i] = kSynthExec[i];
  }
  corrupted[sizeof(kSynthExec) - 1] = 0;
  corrupted[sizeof(kSynthExec) - 2] = 0;
  corrupted[sizeof(kSynthExec) - 3] = 0;
  corrupted[sizeof(kSynthExec) - 4] = 0;

  MangoLinuxLoadResult result = mango_linux_load_and_run(corrupted, sizeof(corrupted));
  if (result.status != MANGO_LINUX_LOAD_INTERP_FAILED) {
    fprintf(stderr,
            "FAIL(reports_undecodable_instructions_without_crashing): status=%d, want "
            "MANGO_LINUX_LOAD_INTERP_FAILED\n",
            result.status);
    return 1;
  }
  printf("ok: an undecodable instruction is reported, not a crash\n");
  return 0;
}

/*
 * A real hand-assembled ARM32 "Hello, World!": adr/mov/svc to write() 14
 * bytes to fd 1, then mov/svc to exit(0). Independently verified with
 * `readelf` the same as the other fixture above; this one actually
 * exercises the write/exit syscall thunking in loader_core.c, not just
 * loading and interpreting.
 */
static const uint8_t kHelloWorld[] = {
    0x7Fu, 0x45u, 0x4Cu, 0x46u, 0x01u, 0x01u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x28u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u,
    0x54u, 0x00u, 0x00u, 0x00u, 0x34u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x34u, 0x00u, 0x20u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x82u, 0x00u, 0x00u, 0x00u,
    0x82u, 0x00u, 0x00u, 0x00u, 0x05u, 0x00u, 0x00u, 0x00u, 0x00u, 0x10u, 0x00u, 0x00u,
    0x18u, 0x10u, 0x8Fu, 0xE2u, 0x01u, 0x00u, 0xA0u, 0xE3u, 0x0Eu, 0x20u, 0xA0u, 0xE3u,
    0x04u, 0x70u, 0xA0u, 0xE3u, 0x00u, 0x00u, 0x00u, 0xEFu, 0x00u, 0x00u, 0xA0u, 0xE3u,
    0x01u, 0x70u, 0xA0u, 0xE3u, 0x00u, 0x00u, 0x00u, 0xEFu, 0x48u, 0x65u, 0x6Cu, 0x6Cu,
    0x6Fu, 0x2Cu, 0x20u, 0x4Du, 0x61u, 0x6Eu, 0x67u, 0x6Fu, 0x21u, 0x0Au,
};

static int test_hello_world_write_and_exit(void) {
  fflush(stdout); /* earlier tests' printfs may still be buffered; flush before redirecting fd 1 */
  int pipefd[2];
  if (pipe(pipefd) != 0) {
    fprintf(stderr, "FAIL(hello_world_write_and_exit): pipe() failed\n");
    return 1;
  }
  int saved_stdout = dup(1);
  dup2(pipefd[1], 1);
  close(pipefd[1]);

  MangoLinuxLoadResult result = mango_linux_load_and_run(kHelloWorld, sizeof(kHelloWorld));

  fflush(stdout);
  dup2(saved_stdout, 1);
  close(saved_stdout);

  char buf[64] = {0};
  ssize_t n = read(pipefd[0], buf, sizeof(buf) - 1);
  close(pipefd[0]);

  if (result.status != MANGO_LINUX_LOAD_OK || result.r0 != 0) {
    fprintf(stderr, "FAIL(hello_world_write_and_exit): status=%d r0=%u, want OK/0\n", result.status,
            result.r0);
    return 1;
  }
  static const char kExpected[] = "Hello, Mango!\n";
  if (n != (ssize_t)(sizeof(kExpected) - 1) || memcmp(buf, kExpected, sizeof(kExpected) - 1) != 0) {
    fprintf(stderr, "FAIL(hello_world_write_and_exit): got %zd bytes, wrong content\n", n);
    return 1;
  }
  printf("ok: a real hand-assembled ARM32 binary writes and exits via real syscall thunking\n");
  return 0;
}

int main(void) {
  int failures = 0;
  failures += test_loads_and_runs_a_real_binary();
  failures += test_rejects_non_elf_input();
  failures += test_reports_undecodable_instructions_without_crashing();
  failures += test_hello_world_write_and_exit();

  if (failures != 0) {
    fprintf(stderr, "%d test(s) failed\n", failures);
    return 1;
  }
  printf("all loader tests passed\n");
  return 0;
}
