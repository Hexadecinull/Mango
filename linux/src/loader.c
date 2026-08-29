/*
 * CLI entry point. The actual loading and running is loader_core.c's
 * mango_linux_load_and_run(); this just reads the file and reports the
 * outcome, see linux/README.md for what "real" means here (loads and
 * runs static, non-PIE ARM32 code; a real syscall like exit() still
 * isn't decodable, so most actual programs stop partway through, not a
 * crash, an expected, reported failure).
 */
#include <stdio.h>
#include <stdlib.h>

#include "mango/linux_loader.h"

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <armeabi-v7a-elf-binary>\n", argv[0]);
    return 2;
  }

  FILE* f = fopen(argv[1], "rb");
  if (!f) {
    fprintf(stderr, "mango_linux_loader: couldn't open '%s'\n", argv[1]);
    return 2;
  }
  if (fseek(f, 0, SEEK_END) != 0) {
    fprintf(stderr, "mango_linux_loader: couldn't seek '%s'\n", argv[1]);
    fclose(f);
    return 2;
  }
  long size = ftell(f);
  if (size < 0 || fseek(f, 0, SEEK_SET) != 0) {
    fprintf(stderr, "mango_linux_loader: couldn't size '%s'\n", argv[1]);
    fclose(f);
    return 2;
  }

  uint8_t* buf = malloc((size_t)size);
  if (!buf || fread(buf, 1, (size_t)size, f) != (size_t)size) {
    fprintf(stderr, "mango_linux_loader: couldn't read '%s'\n", argv[1]);
    fclose(f);
    free(buf);
    return 2;
  }
  fclose(f);

  MangoLinuxLoadResult result = mango_linux_load_and_run(buf, (uint32_t)size);
  free(buf);

  switch (result.status) {
    case MANGO_LINUX_LOAD_OK:
      printf("ran to completion, r0 = %u (0x%x)\n", result.r0, result.r0);
      return 0;
    case MANGO_LINUX_LOAD_BAD_ELF:
      fprintf(stderr, "not a supported ELF32 ARM binary (or not one at all)\n");
      return 1;
    case MANGO_LINUX_LOAD_TOO_LARGE:
      fprintf(stderr, "a segment doesn't fit in this loader's fixed guest memory size\n");
      return 1;
    case MANGO_LINUX_LOAD_INTERP_FAILED:
      fprintf(stderr, "hit an instruction mango_core can't decode yet; not a crash, see linux/README.md\n");
      return 1;
    case MANGO_LINUX_LOAD_UNSUPPORTED_SYSCALL:
      fprintf(stderr, "hit a real syscall (number %u) that isn't thunked yet, see linux/README.md\n",
              result.unsupported_nr);
      return 1;
  }
  return 1;
}
