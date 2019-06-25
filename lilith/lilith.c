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
#include <time.h>

#define DEBUG false
#define DEBUG_LOAD_PASS1 false
#define DEBUG_FILE_SYSTEM false 
#define DEBUG_ALLOC false
#define DEBUG_PRINT_TEMPLEOS_SYMBOL_TABLE_ON_SIGNAL false
#define DEBUG_REGISTER_ALL_ALLOCATIONS false
#define IN_GDB false

#define DRIVE_LETTER 'C'
char DRIVE_ROOT_PATH[] = { DRIVE_LETTER, ':', '\0' };

// tracks EXT_EXTS_NUM defined in KernelA.HH
#define EXT_EXTS_NUM 5

char *templeos_root = NULL;

#include "defs.h"
#include "static.c"
#include "utils.c"
#include "load.c"
#include "hash.c"
#include "task.c"
#include "syscalls.c"
#include "syscalls_tramp.c"
#include "templeos_hash_table.c"

#define RLF_16BIT		0x000001
#define RLF_VGA			0x000002
#define RLF_32BIT		0x000004
#define RLF_PATCHED		0x000008
#define RLF_16MEG_SYS_CODE_BP	0x000010
#define RLF_64BIT		0x000020
#define RLF_16MEG_ADAM_HEAP_CTRL 0x000040
#define RLF_FULL_HEAPS		0x000050
#define RLF_RAW			0x000100
#define RLF_INTERRUPTS		0x000200
#define RLF_BLKDEV		0x000400
#define RLF_MP			0x000800
#define RLF_COMPILER		0x001000
#define RLF_DOC			0x002000
#define RLF_WINMGR		0x004000
#define RLF_REGISTRY		0x008000
#define RLF_HOME		0x010000
#define RLF_AUTO_COMPLETE	0x020000
#define RLF_ADAM_SERVER		0x040000
#define RLF_ONCE_ADAM		0x080000
#define RLF_ONCE_USER		0x100000

#define FUN_SEG_CACHE_SIZE	256
#define FUN_SEG_CACHE_ENTRY_SIZE 64 // sizeof(CFunSegCache)

int main(int argc, char *argv[]) {
	if (argc != 2) {
		fflush(stdout);
		fprintf(stderr, "wrong number of argumens\n");
		exit(EXIT_FAILURE);
	}
	
	templeos_root = getenv("TEMPLEOS");
	
	if ((templeos_root == NULL) || (strlen(templeos_root) == 0)) {
		fprintf(stderr, "TEMPLEOS environment variable is not set!\n");
		templeos_root = NULL;
	}

	hash_init(&symbols, 4096);
	hash_init(&paths_table, 4096);
	
	load_kernel();
	
	struct templeos_thread t;
	init_templeos(&t, &argc);
	
	kernel_patch_var64("adam_task", (uint64_t)(t.Fs));
	
	t.Fs->hash_table = (struct CHashTable *)call_templeos2(&t, "HashTableNew", TASK_HASH_TABLE_SIZE, 0);
	if (DEBUG) {
		printf("adam's hash table %p %lx %lx\n", t.Fs->hash_table, offsetof(struct CTask, hash_table), t.Fs->hash_table->mask);
	}
	
	{ // SysGlblsInit
		call_templeos1(&t, "DbgMode", 1);
		kernel_patch_var64("ext", (uint64_t)(malloc_for_templeos(EXT_EXTS_NUM * sizeof(uint8_t *), false, true)));
		call_templeos(&t, "KeyDevInit");
		
		uint8_t *rev_bits_table = malloc_for_templeos(256, false, true);
		uint8_t *set_bits_table = malloc_for_templeos(256, false, true);
		for (int i = 0; i < 256; ++i) {
			for (int j = 0; j < 8; ++j) {
				if (Bt((uint8_t *)(&i), 7-j)) {
					Bts(rev_bits_table+i, j);
				}
				if (Bt((uint8_t *)(&i), j)) {
					++set_bits_table[i];
				}
			}
		}
		kernel_patch_var64("rev_bits_table", (uint64_t)rev_bits_table);
		kernel_patch_var64("set_bits_table", (uint64_t)set_bits_table);
		
		double *pow10_I64 = malloc_for_templeos(sizeof(double) * (308+308+2), false, true);
		for (int i = -308; i < 309; i++) {
			double f = (double)i;
			uint64_t out = call_templeos1(&t, "_POW10", *((uint64_t *)(&f)));
			pow10_I64[i+309] = *((double *)(&out));
		}
		kernel_patch_var64("pow10_I64", (uint64_t)pow10_I64);
		
		kernel_patch_var64_off("dbg", 0x30, (uint64_t)malloc_for_templeos(FUN_SEG_CACHE_SIZE * FUN_SEG_CACHE_ENTRY_SIZE, false, true));
		kernel_patch_var64_off("dbg", 0x20, call_templeos(&t, "IntFaultHndlrsNew"));
	}
	
	call_templeos(&t, "LoadKernel");
	kernel_patch_var64("adam_task", (uint64_t)(t.Fs));
	call_templeos(&t, "SysDefinesLoad");
	
	if (DEBUG) {
		printf("Initialization Done\n");
	}
	
	kernel_patch_var32("sys_run_level", RLF_64BIT|RLF_RAW|RLF_BLKDEV|RLF_COMPILER);
	
	if (extension_is(argv[1], ".BIN") || extension_is(argv[1], ".BIN.Z")) {
		if (IN_GDB) {
			printf("_HASH_FIND location: %lx\n", hash_get(&symbols, "_HASH_FIND")->val);
			asm("int3;");
		}
		call_templeos1(&t, "DbgMode", 0);
		call_templeos3(&t, "Load", (uint64_t)(argv[1]), 0, INT64_MAX);
	} else if (extension_is(argv[1], ".HC") || extension_is(argv[1], ".HC.Z")) {
		char *p = "/Compiler.BIN.Z";
		call_templeos3(&t, "Load", (uint64_t)p, LDF_SILENT, INT64_MAX);
		call_templeos1(&t, "DbgMode", 0);
		call_templeos2(&t, "ExeFile", (uint64_t)(argv[1]), 0);
	} else {
		fprintf(stderr, "Unknown extension %s\n", argv[1]);
	}
}
