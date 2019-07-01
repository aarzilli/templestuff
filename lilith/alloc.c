#include <immintrin.h>

int *new_spinlock() {
	void *p = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	int *pi = (int *)p;
	*pi = 0;
	return pi;
}

void spin_lock(volatile int *lock) {
	while(!__sync_bool_compare_and_swap(lock, 0, 1)) {
		while(*lock) _mm_pause(); 
	}
}

void spin_unlock(int volatile *lock) {
	asm volatile ("":::"memory"); // acts as a memory barrier.
	*lock = 0;
}

#define STB_MALLOC_IMPLEMENTATION
#include "stb_malloc.h"

stbm_heap *code_heap;
stbm_heap *data_heap;

#define INITIAL_HEAP_SIZE 1*1024*1024

void *code_heap_system_alloc(void *user_context, size_t size_requested, size_t *size_provided) {
	void *p = mmap(NULL, size_requested, PROT_EXEC|PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT, -1, 0);
	if (size_provided != NULL) {
		*size_provided = size_requested;
	}
	register_templeos_memory(p, size_requested, true);
	return p;
}

void *data_heap_system_alloc(void *user_context, size_t size_requested, size_t *size_provided) {
	void *p = mmap(NULL, size_requested, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (size_provided != NULL) {
		*size_provided = size_requested;
	}
	if (DEBUG_REGISTER_ALL_ALLOCATIONS) {
		register_templeos_memory(p, size_requested, false);
	}
	return p;
}

void nop_system_free(void *user_context, void *ptr) {
	// nothing
}

void heaps_init(void) {
	stbm_heap_config code_heap_config;
	memset(&code_heap_config, 0, sizeof(stbm_heap_config));
	code_heap_config.system_alloc = code_heap_system_alloc;
	code_heap_config.system_free = nop_system_free;
	code_heap_config.allocation_mutex = new_spinlock();
	code_heap = stbm_heap_init(code_heap_system_alloc(NULL, INITIAL_HEAP_SIZE, NULL), INITIAL_HEAP_SIZE, &code_heap_config);
	
	stbm_heap_config data_heap_config;
	memset(&data_heap_config, 0, sizeof(stbm_heap_config));
	data_heap_config.system_alloc = data_heap_system_alloc;
	data_heap_config.system_free = nop_system_free;
	data_heap_config.allocation_mutex = new_spinlock();
	data_heap = stbm_heap_init(data_heap_system_alloc(NULL, INITIAL_HEAP_SIZE, NULL), INITIAL_HEAP_SIZE, &data_heap_config);
}
