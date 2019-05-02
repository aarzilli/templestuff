#define _GNU_SOURCE
#define _DEFAULT_SOURCE
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
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <signal.h>
#include <ucontext.h>
#include <stddef.h>
#include <sys/types.h>
#include <dirent.h>

#define DEBUG true
#define DEBUG_LOAD_PASS1 false
#define DEBUG_FILE_FIND true
#define IN_GDB false

#define DRIVE_LETTER 'C'

char *temple_root;

#include "defs.h"
#include "utils.c"
#include "file.c"
#include "load.c"
#include "hash.c"
#include "task.c"

int main(int argc, char *argv[]) {
	if (argc != 2) {
		fflush(stdout);
		fprintf(stderr, "wrong number of argumens\n");
		exit(EXIT_FAILURE);
	}

	temple_root = getenv("TEMPLEOS");
	if ((temple_root == NULL) || (*temple_root == 0)) {
		fflush(stdout);
		fprintf(stderr, "TEMPLEOS environment variable is not set\n");
		exit(EXIT_FAILURE);
	}
	
	char *kernel_path;
	asprintf(&kernel_path, "%s/0000Boot/0000Kernel.BIN.C", temple_root);

	hash_init(&symbols, 4096);
	hash_init(&paths_table, 4096);
	
	load_kernel(kernel_path);
	
	struct templeos_thread t;
	init_templeos(&t, &argc);
	
	kernel_patch_var64("adam_task", (uint64_t)(t.Fs));
	
	call_templeos(&t, "LoadKernel");
	call_templeos(&t, "KeyDevInit");
	
	if (extension_is(argv[1], ".BIN") || extension_is(argv[1], ".BIN.Z")) {
		if (IN_GDB) {
			printf("RedSeaCd location: %lx\n", hash_get(&symbols, "RedSeaCd")->val);
			asm("int3;");
		}
		call_templeos3(&t, "Load", (uint64_t)(argv[1]), 0, INT64_MAX);
	} else if (extension_is(argv[1], ".HC") || extension_is(argv[1], ".HC.Z")) {
		fprintf(stderr, "Not implemented (HolyC)\n");
	} else {
		fprintf(stderr, "Unknown extension %s\n", argv[1]);
	}
}
