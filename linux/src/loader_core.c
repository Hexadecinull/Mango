#include "mango/linux_loader.h"

#include <string.h>
#include <unistd.h>

#include "mango/cpu.h"
#include "mango/elf32.h"
#include "mango/interp.h"

#define MANGO_LINUX_EM_ARM 40u
#define MANGO_LINUX_GUEST_MEM_SIZE (1u * 1024 * 1024) /* fixed size for now, see linux/README.md */
#define MANGO_LINUX_STOP_ADDR 0xFFFFFFFFu
#define MANGO_LINUX_MAX_STEPS 10000u

/* ARM EABI syscall numbers, verified against arch/arm/include/uapi/asm/unistd.h,
 * not guessed: __NR_exit=1, __NR_write=4. */
#define MANGO_LINUX_SYS_EXIT 1u
#define MANGO_LINUX_SYS_WRITE 4u

/* static, not stack-allocated: 1 MiB is too much to put on a thread's
 * stack, and this only ever runs one guest at a time anyway. */
static uint8_t mango_linux_guest_mem[MANGO_LINUX_GUEST_MEM_SIZE];

/* The resume loop: mango_interp_run stops (return 1) at every SVC, hands
 * back r7/r0-r6, and expects PC advanced past it before being called
 * again, see interp.h. This is that expectation, plus the two syscalls
 * linux/README.md promises: real exit and write, everything else stops
 * cleanly instead of guessing what the guest wanted. */
static MangoLinuxLoadResult mango_linux_run_loop(MangoCpu* cpu, MangoMemory* mem) {
  MangoLinuxLoadResult result = {MANGO_LINUX_LOAD_OK, 0, 0};
  for (;;) {
    int rc = mango_interp_run(cpu, mem, MANGO_LINUX_STOP_ADDR, MANGO_LINUX_MAX_STEPS);
    if (rc == 0) {
      result.r0 = cpu->r[0]; /* returned via BX LR to our sentinel, not exit() */
      return result;
    }
    if (rc != 1) {
      result.status = MANGO_LINUX_LOAD_INTERP_FAILED;
      return result;
    }

    uint32_t nr = cpu->r[7];
    if (nr == MANGO_LINUX_SYS_EXIT) {
      result.r0 = cpu->r[0];
      return result;
    }
    if (nr == MANGO_LINUX_SYS_WRITE) {
      uint32_t fd = cpu->r[0];
      uint32_t buf = cpu->r[1];
      uint32_t count = cpu->r[2];
      if ((uint64_t)buf + count > mem->size) {
        cpu->r[0] = (uint32_t)-1; /* not a real errno, just "failed", see linux/README.md */
      } else {
        cpu->r[0] = (uint32_t)write((int)fd, mem->bytes + buf, count);
      }
      cpu->r[MANGO_REG_PC] += 4;
      continue;
    }

    result.status = MANGO_LINUX_LOAD_UNSUPPORTED_SYSCALL;
    result.unsupported_nr = nr;
    return result;
  }
}

MangoLinuxLoadResult mango_linux_load_and_run(const uint8_t* data, uint32_t size) {
  MangoLinuxLoadResult result = {MANGO_LINUX_LOAD_BAD_ELF, 0, 0};
  MangoElf32Image image;
  if (mango_elf32_parse(data, size, MANGO_LINUX_EM_ARM, &image) != 0) {
    return result;
  }

  memset(mango_linux_guest_mem, 0, sizeof(mango_linux_guest_mem));
  for (uint32_t i = 0; i < image.segment_count; i++) {
    const MangoElf32Segment* seg = &image.segments[i];
    if ((uint64_t)seg->vaddr + seg->memsz > sizeof(mango_linux_guest_mem)) {
      result.status = MANGO_LINUX_LOAD_TOO_LARGE;
      return result;
    }
    memcpy(mango_linux_guest_mem + seg->vaddr, data + seg->file_offset, seg->filesz);
  }

  MangoMemory mem = {mango_linux_guest_mem, sizeof(mango_linux_guest_mem)};
  MangoCpu cpu;
  memset(&cpu, 0, sizeof(cpu));
  cpu.r[MANGO_REG_SP] = sizeof(mango_linux_guest_mem) - 64; /* headroom at the very top */
  cpu.r[MANGO_REG_LR] = MANGO_LINUX_STOP_ADDR;
  cpu.r[MANGO_REG_PC] = image.entry;

  return mango_linux_run_loop(&cpu, &mem);
}
