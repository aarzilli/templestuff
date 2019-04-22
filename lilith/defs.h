#ifndef __LILITH_DEFS_H__
#define __LILITH_DEFS_H__

// utils.c //////////////////////////////////////////////////////////////////////////////////////////////////////////
uint64_t read_uint64(void *mem, off_t off);
void *malloc_executable_aligned(size_t size, int64_t alignment, int64_t misalignment);
char *strclone(char *s);

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
int hashfn(char *key);

// file.c //////////////////////////////////////////////////////////////////////////////////////////////////////////
void *load_file(char *path, size_t *psz);

// load.c //////////////////////////////////////////////////////////////////////////////////////////////////////////
extern struct hash_t symbols;

struct export_t {
	uint32_t type;
	uint64_t val;
};

void load_pass1(uint8_t *patch_table, uint8_t *module_base, int64_t ld_flags, bool *pok, void **pentry_p);
void load_one_import(uint8_t **patch_table, uint8_t *module_base, int64_t ld_flags, bool *pok);
struct export_t *symbols_put(char *key, uint32_t type, uint64_t val);

#endif
