#define _GNU_SOURCE
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

#define DEBUG true
#define DEBUG_LOAD_PASS1 false

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

	load_bin(kernel_path, LDF_JUST_LOAD|LDF_KERNEL);
	load_bin(argv[1], 0);
}
