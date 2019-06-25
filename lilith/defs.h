#ifndef __LILITH_DEFS_H__
#define __LILITH_DEFS_H__

// utils.c //////////////////////////////////////////////////////////////////////////////////////////////////////////
extern struct hash_t paths_table;

uint64_t read_uint64(void *mem, off_t off);
void *malloc_executable_aligned(size_t size, int64_t alignment, int64_t misalignment);
char *strclone(char *s);
void writestr(int fd, char *s);
bool extension_is(char *s, char *ext);
void signal_handler(int sig, siginfo_t *info, void *ucontext);
char *fileconcat(char *p1, char *p2, bool for_templeos);

// hash.c //////////////////////////////////////////////////////////////////////////////////////////////////////////
struct hash_t {
	struct hash_entry_t *h;
	int sz;
};

struct hash_entry_t {
	char *key;
	struct hash_entry_t *next;
	struct export_t *val;
};

void hash_init(struct hash_t *h, int sz);
struct export_t *hash_get(struct hash_t *h, char *key);
void hash_put(struct hash_t *h, char *key, struct export_t *val);
struct hash_entry_t *hash_find_closest_entry_before(struct hash_t *h, uint64_t v);
int hashfn(char *key);

// load.c //////////////////////////////////////////////////////////////////////////////////////////////////////////
extern struct hash_t symbols;

struct export_t {
	uint32_t type;
	uint64_t val;
	uint64_t module_base;
};

struct templeos_thread {
	struct CCPU *Gs;
	struct CTask *Fs;
};

void load_pass1(uint8_t *patch_table, uint8_t *module_base);
void load_one_import(uint8_t **patch_table, uint8_t *module_base, int64_t ld_flags);
struct export_t *symbols_put(char *key, uint32_t type, uint64_t val, void *module_base);
void load_kernel(void);
void kernel_patch_instruction(char *name, off_t off, uint8_t original, uint8_t replacement);

// task.c //////////////////////////////////////////////////////////////////////////////////////////////////////////

void init_templeos(struct templeos_thread *t, void *stk_base_estimate);
void enter_templeos(struct templeos_thread *t);
void exit_templeos(struct templeos_thread *t);

uint64_t call_templeos(struct templeos_thread *t, char *name);
uint64_t call_templeos2(struct templeos_thread *t, char *name, uint64_t arg1, uint64_t arg2);
void call_templeos3(struct templeos_thread *t, char *name, uint64_t arg1, uint64_t arg2, uint64_t arg3);

void *malloc_for_templeos(uint64_t size, bool executable, bool zero);
void free_for_templeos(void *p);
void register_templeos_memory(void *p, size_t sz, bool is_mmapped);
struct templeos_mem_entry_t *get_templeos_memory(uint64_t p);
bool is_templeos_memory(uint64_t p);

void trampoline_kernel_patch(char *name, void dest(void));
void kernel_patch_var32(char *name, uint32_t val);
void kernel_patch_var64(char *name, uint64_t val);
void kernel_patch_var64_off(char *name, int off, uint64_t val);

void *find_entry_point(struct templeos_thread *t, char *name);

// templeos_hash_table.c //////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "templeos_defs.h"

struct thiter {
	struct CHashTable *h;
	int i;
	bool first;
	struct CHash *he;
};

struct thiter thiter_new(struct CTask *task);
void thiter_next(struct thiter *it);
bool thiter_valid(struct thiter *it);

bool is_hash_type(struct CHash *he, uint64_t type);

void print_templeos_hash_table(FILE *out, struct CTask *task);

void print_stack_trace(FILE *out, struct CTask *task, uint64_t rip, uint64_t rbp, uint64_t rsp);

// static.c //////////////////////////////////////////////////////////////////////////////////////////////////////////

struct builtin_file {
	char *name;
	uint64_t size;
	uint8_t *body;
};
extern uint8_t kernel_bin_c[];
extern struct builtin_file builtin_files[];

// syscall_tramp.c //////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup_syscall_trampolines(void);

#endif