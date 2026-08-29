#include "mango/elf32.h"

#include <string.h>

#define MANGO_PT_LOAD 1u
#define MANGO_PF_X 1u
#define MANGO_SHT_DYNSYM 11u

static int read_u16(const uint8_t* data, uint32_t size, uint32_t off, uint16_t* out) {
  if (off + 2 > size) {
    return -1;
  }
  *out = (uint16_t)(data[off] | (data[off + 1] << 8));
  return 0;
}

static int read_u32(const uint8_t* data, uint32_t size, uint32_t off, uint32_t* out) {
  if (off + 4 > size) {
    return -1;
  }
  *out = (uint32_t)data[off] | ((uint32_t)data[off + 1] << 8) | ((uint32_t)data[off + 2] << 16) |
         ((uint32_t)data[off + 3] << 24);
  return 0;
}

int mango_elf32_parse(const uint8_t* data, uint32_t size, uint16_t expected_machine, MangoElf32Image* out) {
  uint16_t e_machine, e_phentsize, e_phnum, e_shentsize, e_shnum;
  uint32_t e_entry, e_phoff, e_shoff;

  memset(out, 0, sizeof(*out));
  if (size < 52 || memcmp(data, "\x7f"
                                 "ELF",
                           4) != 0) {
    return -1; /* too small to even hold an ELF32 header, or not an ELF file at all */
  }
  if (data[4] != 1 || data[5] != 1) {
    return -1; /* EI_CLASS != ELFCLASS32, or EI_DATA != little-endian */
  }
  if (read_u16(data, size, 18, &e_machine) != 0 || e_machine != expected_machine) {
    return -1;
  }
  if (read_u32(data, size, 24, &e_entry) != 0 || read_u32(data, size, 28, &e_phoff) != 0 ||
      read_u32(data, size, 32, &e_shoff) != 0) {
    return -1;
  }
  if (read_u16(data, size, 42, &e_phentsize) != 0 || read_u16(data, size, 44, &e_phnum) != 0 ||
      read_u16(data, size, 46, &e_shentsize) != 0 || read_u16(data, size, 48, &e_shnum) != 0) {
    return -1;
  }
  if (e_phentsize < 32 || (e_shnum > 0 && e_shentsize < 40)) {
    return -1; /* phdrs are mandatory; shdrs are optional (e_shnum can legitimately be 0) */
  }

  out->data = data;
  out->size = size;
  out->entry = e_entry;

  for (uint16_t i = 0; i < e_phnum; i++) {
    uint64_t phdr_off = (uint64_t)e_phoff + (uint64_t)i * e_phentsize;
    uint32_t p_type, p_offset, p_vaddr, p_filesz, p_memsz, p_flags;
    if (phdr_off > 0xFFFFFFFFu || phdr_off + 32 > size) {
      return -1;
    }
    if (read_u32(data, size, (uint32_t)phdr_off + 0, &p_type) != 0 ||
        read_u32(data, size, (uint32_t)phdr_off + 4, &p_offset) != 0 ||
        read_u32(data, size, (uint32_t)phdr_off + 8, &p_vaddr) != 0 ||
        read_u32(data, size, (uint32_t)phdr_off + 16, &p_filesz) != 0 ||
        read_u32(data, size, (uint32_t)phdr_off + 20, &p_memsz) != 0 ||
        read_u32(data, size, (uint32_t)phdr_off + 24, &p_flags) != 0) {
      return -1;
    }
    if (p_type != MANGO_PT_LOAD) {
      continue;
    }
    if ((uint64_t)p_offset + p_filesz > size || p_filesz > p_memsz) {
      return -1; /* segment claims bytes the file doesn't have, or shrinks in memory */
    }
    if (out->segment_count >= MANGO_ELF32_MAX_SEGMENTS) {
      return -1; /* more PT_LOAD segments than this project has ever needed to handle */
    }
    MangoElf32Segment* seg = &out->segments[out->segment_count++];
    seg->vaddr = p_vaddr;
    seg->memsz = p_memsz;
    seg->file_offset = p_offset;
    seg->filesz = p_filesz;
    seg->executable = (p_flags & MANGO_PF_X) != 0;
  }

  for (uint16_t i = 0; i < e_shnum; i++) {
    uint64_t shdr_off = (uint64_t)e_shoff + (uint64_t)i * e_shentsize;
    uint32_t sh_type, sh_size, sh_link, sh_entsize, sh_offset;
    if (shdr_off > 0xFFFFFFFFu || shdr_off + 40 > size) {
      return -1;
    }
    if (read_u32(data, size, (uint32_t)shdr_off + 4, &sh_type) != 0) {
      return -1;
    }
    if (sh_type != MANGO_SHT_DYNSYM) {
      continue;
    }
    if (read_u32(data, size, (uint32_t)shdr_off + 16, &sh_offset) != 0 ||
        read_u32(data, size, (uint32_t)shdr_off + 20, &sh_size) != 0 ||
        read_u32(data, size, (uint32_t)shdr_off + 24, &sh_link) != 0 ||
        read_u32(data, size, (uint32_t)shdr_off + 36, &sh_entsize) != 0) {
      return -1;
    }
    if (sh_entsize < 16 || sh_offset + sh_size > size) {
      return -1;
    }
    out->symtab_offset = sh_offset;
    out->symtab_count = sh_size / sh_entsize;

    uint64_t strtab_shdr_off = (uint64_t)e_shoff + (uint64_t)sh_link * e_shentsize;
    uint32_t strtab_offset;
    if (sh_link >= e_shnum || strtab_shdr_off + 40 > size ||
        read_u32(data, size, (uint32_t)strtab_shdr_off + 16, &strtab_offset) != 0) {
      return -1; /* sh_link doesn't point at a real section, .dynsym without .dynstr */
    }
    out->strtab_offset = strtab_offset;
    break; /* one .dynsym is all a normal shared object has */
  }

  return 0;
}

uint32_t mango_elf32_find_symbol(const MangoElf32Image* image, const char* name) {
  for (uint32_t i = 0; i < image->symtab_count; i++) {
    uint32_t sym_off = image->symtab_offset + i * 16;
    uint32_t st_name, st_value;
    if (read_u32(image->data, image->size, sym_off + 0, &st_name) != 0 ||
        read_u32(image->data, image->size, sym_off + 4, &st_value) != 0) {
      continue;
    }
    if (st_name == 0) {
      continue; /* the reserved null symbol every .dynsym starts with */
    }
    uint32_t name_off = image->strtab_offset + st_name;
    if (name_off >= image->size) {
      continue;
    }
    const char* candidate = (const char*)image->data + name_off;
    size_t max_len = image->size - name_off;
    if (strncmp(candidate, name, max_len) == 0 && strlen(candidate) < max_len) {
      return st_value;
    }
  }
  return 0;
}
