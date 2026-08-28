/*
 * Entry point for the real standalone Linux translator: load a 32-bit ARM
 * ELF executable and run it via mango_core, thunking its syscalls to the
 * host instead of emulating them (see linux/README.md, "Design"). Not
 * implemented yet, same honesty as native/src/native_bridge_shim.c's
 * loadLibrary stub: this recognizes the shape of the problem and stops
 * there, on purpose, rather than pretending to work.
 */
#include <stdio.h>

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <armeabi-v7a-elf-binary>\n", argv[0]);
    return 2;
  }

  fprintf(stderr,
          "mango_linux_loader: ELF loading and syscall thunking for '%s' "
          "aren't implemented yet.\nSee linux/README.md and "
          "docs/ARCHITECTURE.md's roadmap; native/'s decoder+interpreter "
          "(mango_core) is ready to be driven from here once those exist.\n",
          argv[1]);
  return 1;
}
