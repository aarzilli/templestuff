#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/mman.h>

#define DEBUG true
#define DEBUG_LOAD_PASS1 false

#include "defs.h"
#include "utils.c"
#include "file.c"
#include "load.c"
#include "hash.c"

int main(int argc, char *argv[]) {
	if (argc != 2) {
		fflush(stdout);
		fprintf(stderr, "wrong number of argumens\n");
		exit(EXIT_FAILURE);
	}

	hash_init(&symbols, 4096);

	size_t sz;
	void *mem = load_file(argv[1], &sz);
	if (DEBUG) {
		printf("loaded at %p\n", mem);

		for (int i = 0; i < 10; i++) {
			printf("%x ", ((unsigned char*)mem)[i]);
		}
		printf("\n");
	}

	// The code that follows is adapted from Load() in D:/Kernel/KLoad.HC.Z

	if (strncmp(((char *)mem)+4, TOSB_SIGNATURE, strlen(TOSB_SIGNATURE)) != 0) {
		fflush(stdout);
		fprintf(stderr, "signature check failed on %s", argv[1]);
		exit(EXIT_FAILURE);
	}

	uint8_t module_align_bits = (uint8_t)(((char *)mem)[2]);
	uint64_t org = read_uint64(mem, 8);
	uint64_t patch_table_offset = read_uint64(mem, 16);
	uint64_t file_size = read_uint64(mem, 16+8);

	int64_t module_align = 1 << module_align_bits;

	if (DEBUG) {
		printf("module_align_bits %x (module_align %lx)\n", module_align_bits, module_align);
		printf("org %lx\n", org);
		printf("patch_table_offset %lx\n", patch_table_offset);
		printf("file_size %lx\n", file_size);
	}

	if (file_size != sz) {
		fflush(stdout);
		fprintf(stderr, "file size mismatch: on disk = %lx in file = %lx\n", sz, file_size);
		exit(EXIT_FAILURE);
	}

	if (!module_align) {
		fflush(stdout);
		fprintf(stderr, "invalid module_align_bits %x\n", module_align_bits);
		exit(EXIT_FAILURE);
	}

	if (org != INT64_MAX) {
		fflush(stdout);
		fprintf(stderr, "can not load non-PIE binaries (org = %lx)\n", org);
		exit(EXIT_FAILURE);
	}

	int64_t misalignment = module_align-TOSB_HEADER_SIZE;
	if (misalignment < 0) misalignment &= module_align-1;
	if (module_align < 16) module_align = 16;

	void *xmem = malloc_executable_aligned(sz, module_align, misalignment);
	memcpy(xmem, mem, sz);
	free(mem);
	mem = xmem;

	if (DEBUG) {
		printf("misalignment %ld (module_align %ld)\n", misalignment, module_align);
		printf("new location at %p\n", xmem);
	}

	void *module_base = (void *)((char *)mem + TOSB_HEADER_SIZE);

	if (DEBUG) {
		printf("module_base %p\n", module_base);
	}
	
	int64_t ld_flags = 0;
	void *entry = NULL;
	bool ok;
	load_pass1((uint8_t *)(mem + patch_table_offset), module_base, ld_flags, &ok, &entry);
	if (DEBUG) {
		printf("Entry Point %p\n", entry);
	}
	if (!ok) {
		fflush(stdout);
		fprintf(stderr, "Load failed due to unresolved symbols.\n");
		exit(EXIT_FAILURE);
	}
	
	if (((ld_flags&LDF_JUST_LOAD) == 0) && (entry != NULL)) {
		//TODO: jump to entry
	}
}
