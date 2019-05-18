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
void print_stack_trace(FILE *out, uint64_t rip, uint64_t rbp);
char *fileconcat(char *p1, char *p2);

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

#define MEM_PAG_BITS 9
#define MEM_FREE_PAG_HASH_SIZE  0x100

struct CBlkPool {
	int64_t   locked_flags, alloced_u8s, used_u8s;
	struct CMemBlk *mem_free_lst,
		*mem_free_2meg_lst, //This is for Sup1CodeScraps/Mem/Mem2Meg.HC.
		*free_pag_hash[MEM_FREE_PAG_HASH_SIZE],
		*free_pag_hash2[64-MEM_PAG_BITS];
};

void init_templeos(struct templeos_thread *t, void *stk_base_estimate);
void enter_templeos(struct templeos_thread *t);
void exit_templeos(struct templeos_thread *t);
void call_templeos(struct templeos_thread *t, char *name);
void call_templeos3(struct templeos_thread *t, char *name, uint64_t arg1, uint64_t arg2, uint64_t arg3);
void *malloc_for_templeos(uint64_t size, bool executable);
void free_for_templeos(void *p);
void register_templeos_memory(void *p, size_t sz, bool is_mmapped);
void *templeos_memory_calloc(size_t sz);
struct templeos_mem_entry_t *get_templeos_memory(uint64_t p);
bool is_templeos_memory(uint64_t p);
void blk_pool_init(struct CBlkPool *bp, int64_t pags);
void blk_pool_add(struct CBlkPool *bp, struct CMemBlk *m, int64_t pags);
struct CHashTable *templeos_hash_table_new(uint64_t size);
struct CHeapCtrl *heap_ctrl_init(struct CBlkPool *bp, struct CTask *task);
void trampoline_kernel_patch(char *name, void dest(void));
void kernel_patch_var64(char *name, uint64_t val);
void print_hash_table(FILE *out, struct CTask *task);

// lilith.s //////////////////////////////////////////////////////////////////////////////////////////////////////////

extern void putchar_asm_wrapper(void);
extern void drvlock_asm_wrapper(void);
extern void templeos_malloc_asm_wrapper(void);
extern void templeos_free_asm_wrapper(void);
extern void redseafilefind_asm_wrapper(void);
extern void redseafileread_asm_wrapper(void);

// static.c //////////////////////////////////////////////////////////////////////////////////////////////////////////

struct builtin_file {
	char *name;
	uint64_t size;
	uint8_t *body;
};

#endif
