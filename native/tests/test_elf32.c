#include <stdio.h>
#include <string.h>

#include "mango/elf32.h"

#define EM_ARM_FOR_TESTS 40u
#define EM_386_FOR_TESTS 3u

/*
 * A hand-built, minimal ELF32 shared object: one PT_LOAD segment plus a
 * .dynsym/.dynstr pair with two exported functions (mango_add at 0x40,
 * mango_answer at 0x50). Independently verified with `readelf` during
 * development (see native/README.md) before trusting it as a test fixture,
 * the same reason test_interp.c cross-checks its own hand-encoded words.
 */
static const uint8_t kSynthElf[] = {
    0x7Fu, 0x45u, 0x4Cu, 0x46u, 0x01u, 0x01u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x03u, 0x00u, 0x28u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x34u, 0x00u, 0x00u, 0x00u, 0x9Cu, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x34u, 0x00u, 0x20u, 0x00u, 0x01u, 0x00u, 0x28u, 0x00u,
    0x03u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x14u, 0x01u, 0x00u, 0x00u,
    0x14u, 0x01u, 0x00u, 0x00u, 0x05u, 0x00u, 0x00u, 0x00u, 0x00u, 0x10u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x40u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x12u, 0x00u, 0x01u, 0x00u, 0x0Bu, 0x00u, 0x00u, 0x00u,
    0x50u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x12u, 0x00u, 0x01u, 0x00u,
    0x00u, 0x6Du, 0x61u, 0x6Eu, 0x67u, 0x6Fu, 0x5Fu, 0x61u, 0x64u, 0x64u, 0x00u, 0x6Du,
    0x61u, 0x6Eu, 0x67u, 0x6Fu, 0x5Fu, 0x61u, 0x6Eu, 0x73u, 0x77u, 0x65u, 0x72u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x04u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x0Bu, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x54u, 0x00u, 0x00u, 0x00u, 0x54u, 0x00u, 0x00u, 0x00u,
    0x30u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x04u, 0x00u, 0x00u, 0x00u, 0x10u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x03u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x84u, 0x00u, 0x00u, 0x00u,
    0x84u, 0x00u, 0x00u, 0x00u, 0x18u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x04u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
};

static int test_parses_a_valid_image(void) {
  MangoElf32Image image;
  int rc = mango_elf32_parse(kSynthElf, sizeof(kSynthElf), EM_ARM_FOR_TESTS, &image);
  if (rc != 0) {
    fprintf(stderr, "FAIL(parses_a_valid_image): mango_elf32_parse returned %d\n", rc);
    return 1;
  }
  if (image.segment_count != 1) {
    fprintf(stderr, "FAIL(parses_a_valid_image): expected 1 PT_LOAD segment, got %u\n",
            image.segment_count);
    return 1;
  }
  if (image.segments[0].vaddr != 0 || image.segments[0].filesz != 0x114 || !image.segments[0].executable) {
    fprintf(stderr, "FAIL(parses_a_valid_image): segment fields wrong (vaddr=%u filesz=%u exec=%d)\n",
            image.segments[0].vaddr, image.segments[0].filesz, image.segments[0].executable);
    return 1;
  }
  printf("ok: parses a valid ELF32 image (1 PT_LOAD segment)\n");
  return 0;
}

static int test_finds_symbols_by_name(void) {
  MangoElf32Image image;
  if (mango_elf32_parse(kSynthElf, sizeof(kSynthElf), EM_ARM_FOR_TESTS, &image) != 0) {
    fprintf(stderr, "FAIL(finds_symbols_by_name): parse failed\n");
    return 1;
  }
  uint32_t add_addr = mango_elf32_find_symbol(&image, "mango_add");
  uint32_t answer_addr = mango_elf32_find_symbol(&image, "mango_answer");
  uint32_t missing = mango_elf32_find_symbol(&image, "does_not_exist");
  if (add_addr != 0x40 || answer_addr != 0x50 || missing != 0) {
    fprintf(stderr, "FAIL(finds_symbols_by_name): add=0x%x answer=0x%x missing=0x%x\n", add_addr,
            answer_addr, missing);
    return 1;
  }
  printf("ok: finds mango_add (0x%x) and mango_answer (0x%x) by name, missing symbol is 0\n", add_addr,
         answer_addr);
  return 0;
}

static int test_rejects_wrong_machine(void) {
  MangoElf32Image image;
  if (mango_elf32_parse(kSynthElf, sizeof(kSynthElf), EM_386_FOR_TESTS, &image) == 0) {
    fprintf(stderr, "FAIL(rejects_wrong_machine): an ARM image was accepted as EM_386\n");
    return 1;
  }
  printf("ok: an ARM image is rejected when a different machine is expected\n");
  return 0;
}

static int test_rejects_truncated_buffer(void) {
  MangoElf32Image image;
  for (uint32_t size = 0; size < 52; size++) {
    if (mango_elf32_parse(kSynthElf, size, EM_ARM_FOR_TESTS, &image) == 0) {
      fprintf(stderr, "FAIL(rejects_truncated_buffer): a %u-byte buffer was accepted\n", size);
      return 1;
    }
  }
  printf("ok: buffers shorter than an ELF32 header are all rejected\n");
  return 0;
}

static int test_rejects_bad_magic(void) {
  uint8_t corrupted[sizeof(kSynthElf)];
  memcpy(corrupted, kSynthElf, sizeof(kSynthElf));
  corrupted[0] = 0x00; /* was 0x7F, the start of the ELF magic */
  MangoElf32Image image;
  if (mango_elf32_parse(corrupted, sizeof(corrupted), EM_ARM_FOR_TESTS, &image) == 0) {
    fprintf(stderr, "FAIL(rejects_bad_magic): a corrupted magic number was accepted\n");
    return 1;
  }
  printf("ok: a corrupted ELF magic number is rejected\n");
  return 0;
}

int main(void) {
  int failures = 0;
  failures += test_parses_a_valid_image();
  failures += test_finds_symbols_by_name();
  failures += test_rejects_wrong_machine();
  failures += test_rejects_truncated_buffer();
  failures += test_rejects_bad_magic();

  if (failures != 0) {
    fprintf(stderr, "%d test(s) failed\n", failures);
    return 1;
  }
  printf("all elf32 tests passed\n");
  return 0;
}
