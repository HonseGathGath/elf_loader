#include "loader.h"
#include <elf.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

static int read_interp_path(FILE *elf, const Elf64_Phdr *phdr, char *interp,
                            size_t interp_size) {
  if (interp_size == 0 || phdr->p_filesz == 0) {
    return 0;
  }

  size_t read_len = phdr->p_filesz;
  if (read_len >= interp_size) {
    read_len = interp_size - 1;
  }

  if (fseek(elf, phdr->p_offset, SEEK_SET) != 0) {
    return 0;
  }

  if (fread(interp, 1, read_len, elf) != read_len) {
    return 0;
  }

  interp[read_len] = '\0';
  return 1;
}

static int map_pt_load_segment(FILE *elf, const Elf64_Phdr *phdr,
                               uintptr_t base_addr, int verbose) {
  if (phdr->p_memsz == 0) {
    return 1;
  }

  size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
  uintptr_t target_vaddr = phdr->p_vaddr + base_addr;
  uintptr_t aligned_addr = target_vaddr & ~(uintptr_t)(page_size - 1U);
  uintptr_t offset = target_vaddr - aligned_addr;
  size_t aligned_size = (size_t)phdr->p_memsz + (size_t)offset;

  int prot = 0;
  if ((phdr->p_flags & PF_R) != 0) {
    prot |= PROT_READ;
  }
  if ((phdr->p_flags & PF_W) != 0) {
    prot |= PROT_WRITE;
  }
  if ((phdr->p_flags & PF_X) != 0) {
    prot |= PROT_EXEC;
  }
  if (prot == 0) {
    prot = PROT_READ;
  }

  void *segment = mmap((void *)aligned_addr, aligned_size,
                       PROT_READ | PROT_WRITE | PROT_EXEC,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

  if (segment == MAP_FAILED) {
    perror("mmap failed in load_elf_image");
    return 0;
  }

  memset(segment, 0, aligned_size);

  if (phdr->p_filesz > 0) {
    if (fseek(elf, phdr->p_offset, SEEK_SET) != 0) {
      perror("fseek failed while loading segment");
      return 0;
    }

    size_t filesz = (size_t)phdr->p_filesz;
    if (fread((char *)segment + offset, 1, filesz, elf) != filesz) {
      fprintf(stderr, "Failed to read PT_LOAD segment contents\n");
      return 0;
    }
  }

  if (mprotect((void *)aligned_addr, aligned_size, prot) != 0) {
    perror("mprotect failed in load_elf_image");
    return 0;
  }

  if (verbose) {
    printf("Mapped PT_LOAD: vaddr=0x%lx size=0x%lx prot=%c%c%c\n",
           (unsigned long)target_vaddr, (unsigned long)phdr->p_memsz,
           (prot & PROT_READ) ? 'r' : '-', (prot & PROT_WRITE) ? 'w' : '-',
           (prot & PROT_EXEC) ? 'x' : '-');
  }

  return 1;
}

int load_elf_image(const char *filepath, uintptr_t base_addr, ElfImageInfo *out,
                   int verbose) {
  if (out == NULL) {
    fprintf(stderr, "load_elf_image: output descriptor cannot be NULL\n");
    return 0;
  }

  memset(out, 0, sizeof(*out));

  FILE *elf = fopen(filepath, "rb");
  if (!elf) {
    perror("Failed to open ELF file");
    return 0;
  }

  Elf64_Ehdr header;
  if (fread(&header, 1, sizeof(header), elf) != sizeof(header)) {
    fprintf(stderr, "Failed to read ELF header from %s\n", filepath);
    fclose(elf);
    return 0;
  }

  if (!(header.e_ident[EI_MAG0] == ELFMAG0 && header.e_ident[EI_MAG1] == ELFMAG1 &&
        header.e_ident[EI_MAG2] == ELFMAG2 && header.e_ident[EI_MAG3] == ELFMAG3)) {
    fprintf(stderr, "%s is not a valid ELF file\n", filepath);
    fclose(elf);
    return 0;
  }

  if (header.e_ident[EI_CLASS] != ELFCLASS64) {
    fprintf(stderr, "%s is not an ELF64 binary\n", filepath);
    fclose(elf);
    return 0;
  }

  out->entry_point = header.e_entry + base_addr;
  out->phnum = header.e_phnum;
  out->phentsize = header.e_phentsize;

  fseek(elf, header.e_phoff, SEEK_SET);
  Elf64_Phdr phdr;
  int found_base_load = 0;

  for (int i = 0; i < header.e_phnum; i++) {
    if (fread(&phdr, 1, sizeof(phdr), elf) != sizeof(phdr)) {
      fprintf(stderr, "Failed to read program header %d from %s\n", i, filepath);
      fclose(elf);
      return 0;
    }

    long next_phdr_pos = ftell(elf);

    if (phdr.p_type == PT_LOAD) {
      if (!found_base_load && phdr.p_offset == 0) {
        out->base_vaddr = phdr.p_vaddr + base_addr;
        found_base_load = 1;
      }

      if (!map_pt_load_segment(elf, &phdr, base_addr, verbose)) {
        fclose(elf);
        return 0;
      }
    } else if (phdr.p_type == PT_PHDR) {
      out->phdr_vaddr = phdr.p_vaddr + base_addr;
    } else if (phdr.p_type == PT_INTERP) {
      if (!read_interp_path(elf, &phdr, out->interp_path, sizeof(out->interp_path))) {
        fprintf(stderr, "Failed to read PT_INTERP path from %s\n", filepath);
        fclose(elf);
        return 0;
      }
    }

    fseek(elf, next_phdr_pos, SEEK_SET);
  }

  if (out->phdr_vaddr == 0 && found_base_load) {
    out->phdr_vaddr = out->base_vaddr + header.e_phoff;
  }

  fclose(elf);

  if (verbose) {
    printf("Loaded ELF image: %s entry=0x%lx base=0x%lx phdr=0x%lx phnum=%u\n",
           filepath, (unsigned long)out->entry_point,
           (unsigned long)out->base_vaddr, (unsigned long)out->phdr_vaddr,
           (unsigned int)out->phnum);
    if (out->interp_path[0] != '\0') {
      printf("ELF requests interpreter: %s\n", out->interp_path);
    }
  }

  return 1;
}

uintptr_t load_elf_segments(const char *filepath, uintptr_t base_addr) {
  ElfImageInfo info;
  if (!load_elf_image(filepath, base_addr, &info, 0)) {
    return 0;
  }
  return info.entry_point;
}
