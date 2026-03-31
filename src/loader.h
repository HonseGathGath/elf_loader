#ifndef LOADER_H
#define LOADER_H

#include <elf.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  uintptr_t entry_point;
  uintptr_t base_vaddr;
  uintptr_t phdr_vaddr;
  uint16_t phnum;
  uint16_t phentsize;
  char interp_path[256];
} ElfImageInfo;

int load_elf_image(const char *filepath, uintptr_t base_addr, ElfImageInfo *out,
                   int verbose);

uintptr_t load_elf_segments(const char *filepath, uintptr_t base_addr);

#endif
