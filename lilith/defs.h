#ifndef __LILITH_DEFS_H__
#define __LILITH_DEFS_H__

// alloc.c //////////////////////////////////////////////////////////////////////////////////////////////////////////

#define STBM_MUTEX_HANDLE int *
#define STBM_MUTEX_ACQUIRE spin_lock
#define STBM_MUTEX_RELEASE spin_unlock
#define STBM_UINT32 uint32_t
#define STBM_UINTPTR uintptr_t
#define STBM_POINTER_SIZE 64
#define STBM_ASSERT(x) { if(!(x)) _exit(1); }
#define STBM_MEMSET memset
#define STBM_MEMCPY memcpy
#include "stb_malloc.h"

extern stbm_heap *data_heap;
extern stbm_heap *code_heap;
void heaps_init(void);

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

struct CDC {}; // we needn't concer with this class

#define STR_LEN 144
#define TASK_NAME_LEN 32
#define TASK_EXCEPT_CALLERS 8

struct CJobCtrl {
	void *next_waiting, *last_waiting;
	void *next_done, *last_done;
	int64_t flags;
};

struct CWinScroll {
	int64_t   min,pos,max;
	uint32_t   flags;
	uint8_t    color,pad[3];
};

struct CTask { //The Fs segment reg points to current CTask.
	struct CTask *addr; //Self-addressed ptr
	uint32_t   task_signature,win_inhibit;
	int64_t   wake_jiffy;
	uint32_t   task_flags,display_flags;
	
	uint64_t code_heap, data_heap;
	
	struct CDoc  *put_doc,*display_doc, //When double buffering, these two differ.
	      *border_doc;
	int64_t   win_left,win_right,win_top,win_bottom;
	
	struct CDrv  *cur_dv;
	uint8_t    *cur_dir;
	
	struct CTask *parent_task,
	      *next_task,*last_task,
	      *next_input_filter_task,*last_input_filter_task,
	      *next_sibling_task,*last_sibling_task,
	      *next_child_task,*last_child_task;
	
	//These are derived from left,top,right,bottom
	int64_t   win_width,win_height,
	      pix_left,pix_right,pix_width, //These are in pixs, not characters
	      pix_top,pix_bottom,pix_height,
	      scroll_x,scroll_y,scroll_z;
	
	//These must be in this order
	//for $LK,"TASK_CONTEXT_SAVE",A="FF:::/Kernel/Sched.HC,TASK_CONTEXT_SAVE"$ and $LK,"_TASK_CONTEXT_RESTORE",A="FF:::/Kernel/Sched.HC,_TASK_CONTEXT_RESTORE"$
	int64_t   rip,rflags,rsp,rsi,rax,rcx,rdx,rbx,rbp,rdi,
	      r8,r9,r10,r11,r12,r13,r14,r15;
	struct CCPU  *gs;
	struct CFPU  *fpu_mmx;
	int64_t   swap_cnter;
	
	void    (*draw_it)(struct CTask *task, struct CDC *dc);
	
	uint8_t    task_title[STR_LEN],
	      task_name[TASK_NAME_LEN],
	      wallpaper_data[STR_LEN],
	
	      title_src,border_src,
	      text_attr,border_attr;
	uint16_t   win_z_num,pad;
	
	struct CTaskStk *stk;
	struct CExcept *next_except,*last_except;
	int64_t   except_rbp,     //throw routine's RBP
	      except_ch;      //throw(ch)
	uint8_t    *except_callers[TASK_EXCEPT_CALLERS];
	
	bool  catch_except;
	bool  new_answer;
	uint8_t    answer_type, pad2[5];
	int64_t   answer;
	double   answer_time;
	struct CBpt  *bpt_lst;
	struct CCtrl *next_ctrl,*last_ctrl;
	struct CMenu *cur_menu;
	struct CTaskSettings *next_settings;
	struct CMathODE *next_ode,*last_ode;
	double   last_ode_time;
	struct CHashTable *hash_table;
	
	struct CJobCtrl srv_ctrl;
	struct CCmpCtrl *next_cc,*last_cc;
	struct CHashFun *last_fun;
	
	void    (*task_end_cb)();
	struct CTask *song_task,*animate_task;
	int64_t   rand_seed,
	      task_num,
	      fault_num,fault_err_code;
	struct CTask *popup_task,
	      *dbg_task;
	struct CWinScroll horz_scroll,vert_scroll;
	
	int64_t   user_data;
};

void init_templeos(struct templeos_thread *t, void *stk_base_estimate);
void enter_templeos(struct templeos_thread *t);
void exit_templeos(struct templeos_thread *t);

uint64_t call_templeos(struct templeos_thread *t, char *name);
uint64_t call_templeos2(struct templeos_thread *t, char *name, uint64_t arg1, uint64_t arg2);
void call_templeos3(struct templeos_thread *t, char *name, uint64_t arg1, uint64_t arg2, uint64_t arg3);

void *malloc_for_templeos(uint64_t size, stbm_heap *heap, bool zero);
void free_for_templeos(void *p);
void register_templeos_memory(void *p, size_t sz, bool is_mmapped);
struct templeos_mem_entry_t *get_templeos_memory(uint64_t p);
bool is_templeos_memory(uint64_t p);

void trampoline_kernel_patch(char *name, void dest(void));
void kernel_patch_var32(char *name, uint32_t val);
void kernel_patch_var64(char *name, uint64_t val);
void kernel_patch_var64_off(char *name, int off, uint64_t val);
uint64_t *kernel_var64_ptr(char *name);

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

int print_stack_trace(FILE *out, struct CTask *task, uint64_t rip, uint64_t rbp, uint64_t rsp);

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

// x11.c //////////////////////////////////////////////////////////////////////////////////////////////////////////

extern char *x11_display;
extern bool x11_enabled;


#endif