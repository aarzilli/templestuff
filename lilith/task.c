#define ARCH_SET_GS             0x1001
#define ARCH_SET_FS             0x1002
#define ARCH_GET_FS             0x1003
#define ARCH_GET_GS             0x1004

struct CCPU { //The Gs segment reg points to current CCPU.
	struct CCPU *addr; //Self-addressed ptr
	int64_t num, cpu_flags, startup_rip, idle_pt_hits;
	double idle_factor;
	int64_t total_jiffies;
	struct CTask *seth_task,*idle_task;
	int64_t tr, swap_cnter;
	void (*profiler_timer_irq)(struct CTask *task);
	struct CTaskDying *next_dying, *last_dying;
	int64_t kill_jiffy;
	struct CTSS  *tss;
	int64_t start_stk[16];
	
	uint64_t glibc_fs, glibc_gs;
};

struct CTSS {
	uint32_t res1;
	int64_t rsp0,rsp1,rsp2,res2,
	      ist1,ist2,ist3,ist4,ist5,ist6,ist7,res3;
	uint16_t res4,io_map_offset;
	uint8_t io_map[0x10000/8];
	int64_t *st0,*st1,*st2;
	uint16_t tr,tr_ring3;
};

#define STR_LEN 144
#define TASK_NAME_LEN 32
#define TASK_EXCEPT_CALLERS 8

struct CDC {}; // we needn't concer with this class

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
	
	struct CHeapCtrl *code_heap,*data_heap;
	
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

int arch_prctl(int code, unsigned long addr) {
	return syscall(SYS_arch_prctl, code, addr);
}

void init_templeos(struct templeos_thread *t) {
	t->Gs = (struct CCPU *)calloc(1, sizeof(struct CCPU));
	t->Gs->addr = t->Gs;
	t->Fs = (struct CTask *)calloc(1, sizeof(struct CTask));
	t->Fs->addr = t->Fs;
	t->Fs->gs = t->Gs;
}
	
void enter_templeos(struct templeos_thread *t) {
	if ((t->Gs == NULL) || (t->Fs == NULL)) {
		abort();
	}
	char *error_phase = "none";
	
	fflush(stdout);
	fflush(stderr);
	
	if (arch_prctl(ARCH_GET_GS, (uint64_t)&(t->Gs->glibc_gs)) == -1) {
		error_phase = "saving GS base";
		goto enter_templeos_failed;
		
	}
	if (arch_prctl(ARCH_GET_FS, (uint64_t)&(t->Gs->glibc_fs)) == -1) {
		error_phase = "saving FS base";
		goto enter_templeos_failed;
		
	}

	if (arch_prctl(ARCH_SET_GS, (uint64_t)(t->Gs)) == -1) {
		error_phase = "set GS base";
		goto enter_templeos_failed;
	}
	if (arch_prctl(ARCH_SET_FS, (uint64_t)(t->Fs)) == -1) {
		error_phase = "set FS base";
		goto enter_templeos_failed;
	}
	
	// sanity check
	uint64_t gs_sanity, fs_sanity;
	if (arch_prctl(ARCH_GET_GS, (uint64_t)&gs_sanity) == -1) {
		error_phase = "get GS base for sanity check";
		goto enter_templeos_failed;
	}
	if (arch_prctl(ARCH_GET_FS, (uint64_t)&fs_sanity) == -1) {
		error_phase = "get GS base for sanity check";
		goto enter_templeos_failed;
	}
	
	if (gs_sanity != (uint64_t)(t->Gs)) {
		error_phase = "sanity check GS base";
		goto enter_templeos_failed;
	}
	if (fs_sanity != (uint64_t)(t->Fs)) {
		error_phase = "sanity check FS base";
		goto enter_templeos_failed;
	}
	
	return;

enter_templeos_failed:
	writestr(2, "could not ");
	writestr(2, error_phase);
	writestr(2, "\n");
	// no point trying to access errno here, it's probably broken
	_exit(EXIT_FAILURE);
}

void exit_templeos(struct templeos_thread *t) {
	char *error_phase = "none";

	if (arch_prctl(ARCH_GET_GS, (uint64_t)&(t->Gs)) == -1) {
		error_phase = "get GS base";
		goto exit_templeos_failed;
	}
	if (arch_prctl(ARCH_GET_GS, (uint64_t)&(t->Fs)) == -1) {
		error_phase = "get GS base";
		goto exit_templeos_failed;
	}
	
	if (arch_prctl(ARCH_SET_GS, (uint64_t)(t->Gs->glibc_gs)) == -1) {
		error_phase = "set GS base";
		goto exit_templeos_failed;
	}
	if (arch_prctl(ARCH_SET_FS, (uint64_t)(t->Gs->glibc_fs)) == -1) {
		error_phase = "set FS base";
		goto exit_templeos_failed;
	}
	
	return;
	
exit_templeos_failed:
	writestr(2, "could not ");
	writestr(2, error_phase);
	writestr(2, "\n");
	// no point trying to access errno here, it's probably broken
	_exit(EXIT_FAILURE);
}

struct templeos_mem_entry_t {
	void *mem;
	uint64_t sz;
};

struct templeos_mem_map_t {
	struct templeos_mem_entry_t *v;
	uint64_t cap;
	uint64_t len;
} templeos_mem_map = { NULL, 0, 0 };

#define TEMPLEOS_MEM_MAP_INITIAL_SIZE 20

void register_templeos_memory(void *p, size_t sz) {
	if (templeos_mem_map.v == NULL) {
		templeos_mem_map.v = (struct templeos_mem_entry_t *)calloc(TEMPLEOS_MEM_MAP_INITIAL_SIZE, sizeof(struct templeos_mem_entry_t));
		templeos_mem_map.cap = TEMPLEOS_MEM_MAP_INITIAL_SIZE;
		templeos_mem_map.len = 0;
	}
	
	if (templeos_mem_map.len == templeos_mem_map.cap) {
		templeos_mem_map.v = realloc(templeos_mem_map.v, sizeof(struct templeos_mem_entry_t) * templeos_mem_map.cap * 2);
	}
	templeos_mem_map.v[templeos_mem_map.len].mem = p;
	templeos_mem_map.v[templeos_mem_map.len].sz = sz;
	++templeos_mem_map.len;
}

bool is_templeos_memory(uint64_t p) {
	for (int i = 0; i < templeos_mem_map.len; ++i) {
		struct templeos_mem_entry_t e = templeos_mem_map.v[i];
		if ((((uint64_t)e.mem) <= p) && (p < (((uint64_t)e.mem) + e.sz))) {
			return true;
		}
	}
	return false;
}
