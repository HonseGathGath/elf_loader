#include "loader.h"
#include <elf.h>
#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/random.h>
#include <unistd.h>

typedef struct {
  int verbose;
  size_t stack_size;
  uintptr_t interp_base;
  const char *elf_path;
} LoaderOptions;

static void print_usage(const char *prog) {
  fprintf(stderr,
          "Usage: %s [--verbose] [--stack-size <bytes>] "
          "[--interp-base <hex|dec>] <elf_executable>\n",
          prog);
}

static int parse_options(int argc, char *argv[], LoaderOptions *opts) {
  opts->verbose = 0;
  opts->stack_size = 1024 * 1024;
  opts->interp_base = 0x7f5500000000ULL;
  opts->elf_path = NULL;

  for (int argi = 1; argi < argc; argi++) {
    if (strcmp(argv[argi], "--verbose") == 0) {
      opts->verbose = 1;
      continue;
    }

    if (strcmp(argv[argi], "--stack-size") == 0) {
      if (argi + 1 >= argc) {
        fprintf(stderr, "Missing value for --stack-size\n");
        return 0;
      }

      errno = 0;
      char *end = NULL;
      unsigned long long parsed = strtoull(argv[++argi], &end, 10);
      if (errno != 0 || end == argv[argi] || *end != '\0' || parsed < 4096ULL) {
        fprintf(stderr,
                "Invalid --stack-size value '%s' (must be integer >= 4096)\n",
                argv[argi]);
        return 0;
      }
      opts->stack_size = (size_t)parsed;
      continue;
    }

    if (strcmp(argv[argi], "--interp-base") == 0) {
      if (argi + 1 >= argc) {
        fprintf(stderr, "Missing value for --interp-base\n");
        return 0;
      }

      errno = 0;
      char *end = NULL;
      unsigned long long parsed = strtoull(argv[++argi], &end, 0);
      if (errno != 0 || end == argv[argi] || *end != '\0') {
        fprintf(stderr, "Invalid --interp-base value '%s'\n", argv[argi]);
        return 0;
      }
      opts->interp_base = (uintptr_t)parsed;
      continue;
    }

    if (argv[argi][0] == '-') {
      fprintf(stderr, "Unknown option: %s\n", argv[argi]);
      return 0;
    }

    if (opts->elf_path != NULL) {
      fprintf(stderr, "Only one ELF executable path is supported\n");
      return 0;
    }
    opts->elf_path = argv[argi];
  }

  return opts->elf_path != NULL;
}

static void fill_random_bytes(uint8_t random_bytes[16]) {
  ssize_t got = getrandom(random_bytes, 16, 0);
  if (got == 16) {
    return;
  }

  for (int i = 0; i < 16; i++) {
    random_bytes[i] = (uint8_t)(0x10 + i);
  }
}

int main(int argc, char *argv[]) {
  LoaderOptions options;

  if (!parse_options(argc, argv, &options)) {
    print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  if (options.verbose) {
    printf("[loader] main=%s stack_size=%zu interp_base=0x%" PRIxPTR "\n",
           options.elf_path, options.stack_size, options.interp_base);
  }

  ElfImageInfo main_image;
  if (!load_elf_image(options.elf_path, 0, &main_image, options.verbose)) {
    return EXIT_FAILURE;
  }

  uint8_t random_bytes[16];
  fill_random_bytes(random_bytes);

  printf("Main executable loaded! Entry point: 0x%lx\n",
         (unsigned long)main_image.entry_point);

  uintptr_t interp_entry = 0;
  ElfImageInfo interp_image;
  memset(&interp_image, 0, sizeof(interp_image));

  if (main_image.interp_path[0] != '\0') {
    printf("Loading interpreter: %s\n", main_image.interp_path);
    if (!load_elf_image(main_image.interp_path, options.interp_base, &interp_image,
                        options.verbose)) {
      return EXIT_FAILURE;
    }
    interp_entry = interp_image.entry_point;
  }

  void *stack_region = mmap(NULL, options.stack_size, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (stack_region == MAP_FAILED) {
    perror("Failed to map synthetic stack");
    return EXIT_FAILURE;
  }

  uint64_t *fake_stack =
      (uint64_t *)((char *)stack_region + options.stack_size / 2);

  int i = 0;

  fake_stack[i++] = 1;
  fake_stack[i++] = (uint64_t)options.elf_path;
  fake_stack[i++] = 0;
  fake_stack[i++] = 0;

  Elf64_auxv_t *auxv = (Elf64_auxv_t *)&fake_stack[i];
  int aux_idx = 0;

  auxv[aux_idx].a_type = AT_PHDR;
  auxv[aux_idx++].a_un.a_val = main_image.phdr_vaddr;

  auxv[aux_idx].a_type = AT_PHENT;
  auxv[aux_idx++].a_un.a_val = main_image.phentsize;

  auxv[aux_idx].a_type = AT_PHNUM;
  auxv[aux_idx++].a_un.a_val = main_image.phnum;

  auxv[aux_idx].a_type = AT_PAGESZ;
  auxv[aux_idx++].a_un.a_val = sysconf(_SC_PAGESIZE);

  auxv[aux_idx].a_type = AT_ENTRY;
  auxv[aux_idx++].a_un.a_val = main_image.entry_point;

  auxv[aux_idx].a_type = AT_BASE;
  auxv[aux_idx++].a_un.a_val = (interp_entry != 0) ? options.interp_base : 0;

  auxv[aux_idx].a_type = AT_RANDOM;
  auxv[aux_idx++].a_un.a_val = (uintptr_t)random_bytes;

  auxv[aux_idx].a_type = AT_EXECFN;
  auxv[aux_idx++].a_un.a_val = (uintptr_t)options.elf_path;

  auxv[aux_idx].a_type = AT_SECURE;
  auxv[aux_idx++].a_un.a_val = 0;

  auxv[aux_idx].a_type = AT_NULL;
  auxv[aux_idx++].a_un.a_val = 0;

  uintptr_t jump_target =
      (interp_entry != 0) ? interp_entry : main_image.entry_point;

  printf("Handing off control to: 0x%lx\n", jump_target);

  __asm__ volatile("mov %0, %%rsp\n"
                   // "xor %%rax, %%rax\n"
                   // "xor %%rbx, %%rbx\n"
                   // "xor %%rcx, %%rcx\n"
                   "xor %%rdx, %%rdx\n"
                   "jmp *%1\n"
                   :
                   : "r"(fake_stack), "a"(jump_target)
                   : "memory");

  return EXIT_SUCCESS;
}
