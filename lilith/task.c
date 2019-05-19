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

struct CDrv {
  int64_t   locked_flags;
  uint32_t   dv_signature;
  uint8_t    drv_let,pad;
  uint16_t   fs_type;
  int64_t   drv_offset,
        size,
        prt_num,
        file_system_info_sect,
        fat1,fat2,
        root_clus,
        data_area,
        spc; //sectors per clus
  int64_t fat32_local_time_offset;
  struct CTask *owning_task;
  struct CBlkDev *bd;

  struct CFAT32FileInfoSect *fis;
  int64_t   fat_blk_dirty,
        cur_fat_blk_num;
  uint32_t   *cur_fat_blk;
  struct CFreeLst *next_free,*last_free;
};

struct CBlkDev {
  struct CBlkDev *lock_fwding; //If two blkdevs on same controller, use just one lock
  int64_t   locked_flags;
  uint32_t   bd_signature,
        type,flags;
  uint8_t    first_drv_let,unit,pad[2];
  uint32_t   base0,base1,
        blk_size;
  int64_t   drv_offset,init_root_dir_blks,
        max_blk;
  uint16_t   *dev_id_record;
  uint8_t    *RAM_dsk,
        *file_dsk_name;
  struct CFile *file_dsk;
  struct CTask *owning_task;
  double   last_time;
  uint32_t   max_reads,max_writes;
};

struct CBlkDevGlbls {
  struct CBlkDev *blkdevs;
  uint8_t    *dft_iso_filename;      //$TX,"\"::/Tmp/CDDVD.ISO\"",D="DFT_ISO_FILENAME"$
  uint8_t    *dft_iso_c_filename;    //$TX,"\"::/Tmp/CDDVD.ISO.C\"",D="DFT_ISO_C_FILENAME"$
  uint8_t    *tmp_filename;
  uint8_t    *home_dir;
  struct CCacheBlk *cache_base,*cache_ctrl,**cache_hash_table;
  int64_t   cache_size,read_cnt,write_cnt;
  struct CDrv  *drvs,*let_to_drv[32];
  int64_t   mount_ide_auto_cnt,
        ins_base0,ins_base1;    //Install cd/dvd controller.
  uint8_t    boot_drv_let,first_hd_drv_let,first_dvd_drv_let;
  uint8_t  dvd_boot_is_good,ins_unit,pad[3];
};

struct CTaskStk {
  struct CTaskStk *next_stk;
  int64_t   stk_size,stk_ptr;
  void *    stk_base;
};


int arch_prctl(int code, unsigned long addr) {
	return syscall(SYS_arch_prctl, code, addr);
}

#define TASK_HASH_TABLE_SIZE	(1<<10)

void init_templeos(struct templeos_thread *t, void *stk_base_estimate) {
	t->Gs = (struct CCPU *)calloc(1, sizeof(struct CCPU));
	t->Gs->addr = t->Gs;
	t->Fs = (struct CTask *)calloc(1, sizeof(struct CTask));
	t->Fs->addr = t->Fs;
	t->Fs->gs = t->Gs;
	t->Fs->task_signature = 0x536b7354; //TskS
	
	t->Fs->data_heap = 0x1;
	t->Fs->code_heap = 0x2;
	
	t->Fs->cur_dv = malloc_for_templeos(sizeof(struct CDrv), false, true);
	t->Fs->cur_dv->dv_signature = 0x56535644;
	t->Fs->cur_dv->fs_type = 1; // we're going to impersonate the RedSea file system (because it's easier)
	t->Fs->cur_dv->drv_let = DRIVE_LETTER;
	t->Fs->cur_dv->bd = malloc_for_templeos(sizeof(struct CBlkDev), false, true);
	t->Fs->cur_dv->bd->bd_signature = 0x56534442;
	t->Fs->cur_dv->bd->type = 1; // Ram disk
	char *w = get_current_dir_name();
	t->Fs->cur_dir = (uint8_t *)fileconcat(DRIVE_ROOT_PATH, w, true);
	free(w);
	
	struct export_t *blkdev_export = hash_get(&symbols, "blkdev");
	if (blkdev_export == NULL) {
		fprintf(stderr, "Could not find global variable blkdev\n");
		exit(EXIT_FAILURE);
	}
	struct CBlkDevGlbls *blkdev = (struct CBlkDevGlbls *)(blkdev_export->val);
	blkdev->boot_drv_let = DRIVE_LETTER;
	blkdev->first_hd_drv_let = DRIVE_LETTER;
	char *homep = fileconcat(DRIVE_ROOT_PATH, getenv("HOME"), true);
	blkdev->home_dir = (uint8_t *)homep;
	blkdev->let_to_drv[DRIVE_LETTER - 'A'] = t->Fs->cur_dv;
	
	// que_init
	t->Fs->next_except = (struct CExcept *)(&(t->Fs->next_except));
	t->Fs->last_except = (struct CExcept *)(&(t->Fs->next_except));
	
	// que_init
	t->Fs->next_cc = (struct CCmpCtrl *)(&(t->Fs->next_cc));
	t->Fs->last_cc = (struct CCmpCtrl *)(&(t->Fs->next_cc));
	
	// que_init
	t->Fs->srv_ctrl.next_waiting = (void *)&(t->Fs->srv_ctrl.next_waiting);
	t->Fs->srv_ctrl.last_waiting = (void *)&(t->Fs->srv_ctrl.last_waiting);
	
	// que_init
	t->Fs->srv_ctrl.next_done = (void *)&(t->Fs->srv_ctrl.next_done);
	t->Fs->srv_ctrl.last_done = (void *)&(t->Fs->srv_ctrl.last_done);
	
	t->Fs->stk = malloc_for_templeos(sizeof(struct CTaskStk), false, true);
	t->Fs->stk->stk_base = stk_base_estimate;
	t->Fs->stk->stk_size = 4 * 1024 * 1024;
	
	struct sigaction act;
	
	act.sa_sigaction = signal_handler;
	memset(&(act.sa_mask), 0, sizeof(sigset_t));
	act.sa_flags = SA_SIGINFO;
	
	sigaction(SIGILL, &act, NULL);
	sigaction(SIGABRT, &act, NULL);
	sigaction(SIGBUS, &act, NULL);
	sigaction(SIGFPE, &act, NULL);
	sigaction(SIGABRT, &act, NULL);
	sigaction(SIGSEGV, &act, NULL);
	sigaction(SIGTRAP, &act, NULL);
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
	if (arch_prctl(ARCH_GET_FS, (uint64_t)&(t->Fs)) == -1) {
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
	bool used;
	bool is_mmapped;
};

struct templeos_mem_map_t {
	struct templeos_mem_entry_t *v;
	uint64_t cap;
	uint64_t len;
} templeos_mem_map = { NULL, 0, 0 };

#define TEMPLEOS_MEM_MAP_INITIAL_SIZE 20

void register_templeos_memory(void *p, size_t sz, bool is_mmapped) {
	//TODO: register_templeos_memory is inefficient (maybe we shouldn't even call it?)

	if (templeos_mem_map.v == NULL) {
		templeos_mem_map.v = (struct templeos_mem_entry_t *)calloc(TEMPLEOS_MEM_MAP_INITIAL_SIZE, sizeof(struct templeos_mem_entry_t));
		templeos_mem_map.cap = TEMPLEOS_MEM_MAP_INITIAL_SIZE;
		templeos_mem_map.len = 0;
	}
	
	if (templeos_mem_map.len == templeos_mem_map.cap) {
		templeos_mem_map.cap *= 2;
		templeos_mem_map.v = realloc(templeos_mem_map.v, sizeof(struct templeos_mem_entry_t) * templeos_mem_map.cap);
	}
	templeos_mem_map.v[templeos_mem_map.len].used = true;
	templeos_mem_map.v[templeos_mem_map.len].mem = p;
	templeos_mem_map.v[templeos_mem_map.len].sz = sz;
	templeos_mem_map.v[templeos_mem_map.len].is_mmapped = is_mmapped;
	++templeos_mem_map.len;
}

void unregister_templeos_memory(uint64_t p) {
	struct templeos_mem_entry_t *e = get_templeos_memory(p);
	e->used = false;
}

struct templeos_mem_entry_t *get_templeos_memory(uint64_t p) {
	for (int i = 0; i < templeos_mem_map.len; ++i) {
		struct templeos_mem_entry_t e = templeos_mem_map.v[i];
		if (e.used && (((uint64_t)e.mem) <= p) && (p < (((uint64_t)e.mem) + e.sz))) {
			return &templeos_mem_map.v[i];
		}
	}
	return NULL;
}

bool is_templeos_memory(uint64_t p) {
	struct templeos_mem_entry_t *e = get_templeos_memory(p);
	return e != NULL;
}

void *find_entry_point(struct templeos_thread *t, char *name) {
	struct export_t *e = hash_get(&symbols, name);
	if (e != NULL) {
		return (void *)(e->val);
	}
	
	for (struct thiter it = thiter_new(t->Fs); thiter_valid(&it); thiter_next(&it)) {
		if (!is_hash_type(it.he, HTT_EXPORT_SYS_SYM|HTF_IMM)) {
			continue;
		}
		if (strcmp((char *)(it.he->str), name) != 0) {
			continue;
		}
		
		struct CHashExport *ex = (struct CHashExport *)(it.he);
		return (void *)(ex->val);
	}
	return NULL;
}

void call_templeos(struct templeos_thread *t, char *name) {
	void *entry = find_entry_point(t, name);
	if (entry == NULL) {
		fprintf(stderr, "Could not call %s\n", name);
		exit(EXIT_FAILURE);
	}
	fflush(stdout);
	fflush(stderr);
	
	enter_templeos(t);
	((void (*)(void))entry)();
	exit_templeos(t);
}

extern uint64_t call_templeos2_asm(void *entry, uint64_t arg1, uint64_t arg2);

uint64_t call_templeos2(struct templeos_thread *t, char *name, uint64_t arg1, uint64_t arg2) {
	void *entry = find_entry_point(t, name);
	if (entry == NULL) {
		fprintf(stderr, "Could not call %s\n", name);
		exit(EXIT_FAILURE);
	}
	fflush(stdout);
	fflush(stderr);
	
	enter_templeos(t);
	uint64_t r = call_templeos2_asm(entry, arg1, arg2);
	exit_templeos(t);
	return r;
}

extern void call_templeos3_asm(void *entry, uint64_t arg1, uint64_t arg2, uint64_t arg3);

void call_templeos3(struct templeos_thread *t, char *name, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
	void *entry = find_entry_point(t, name);
	if (entry == NULL) {
		fprintf(stderr, "Could not call %s\n", name);
		exit(EXIT_FAILURE);
	}
	fflush(stdout);
	fflush(stderr);
	
	enter_templeos(t);
	call_templeos3_asm(entry, arg1, arg2, arg3);
	exit_templeos(t);
}

// trampoline_kernel_patch writes a jump to 'dest' at the entry point of the kernel function named 'name'.
// the jump in question is an absolute 64bit jump constructed using a PUSH+MOV+RET sequence.
void trampoline_kernel_patch(char *name, void dest(void)) {
	uint8_t *x = (uint8_t *)(hash_get(&symbols, name)->val);
	uint64_t d = (uint64_t)dest;
	if (DEBUG) {
		printf("patching %p as jump to %lx\n", x, d);
	}
	x[0] = 0x68; // PUSH <lower 32 bits>
	*((uint32_t *)(x+1)) = (uint32_t)d;
	x[5] = 0xc7; // MOV <higher 32 bits>
	x[6] = 0x44;
	x[7] = 0x24;
	x[8] = 0x04;
	*((uint32_t *)(x+9)) = (uint32_t)(d>>32);
	x[13] = 0xc3; // RETx
	
	//TODO: switch to 
	// JMP QWORD PTR [RIP+0]
	// <bytes of dest>
	// 0xff 0x25 0x00 0x00 0x00 0x00 (...)
	// same size but more standard?
}

// malloc_for_templeos returns a chunk of memory of the specified size
// allocated for TempleOS (TempleOS will be able to call Free on it).
void *malloc_for_templeos(uint64_t size, bool executable, bool zero) {
	void *p;
	bool is_mmapped = false;
	if (executable) {
		p = mmap(NULL, size, PROT_EXEC|PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT, -1, 0);
		is_mmapped = true;
	} else {
		p = malloc(size);
	}
	if (zero) {
		memset(p, 0, size);
	}
	register_templeos_memory(p, size, is_mmapped);
	return p;
}

// free_for_templeos frees memory allocated for TempleOS (p could have been
// allocated by calling MAlloc).
void free_for_templeos(void *p) {
	if (p == NULL) {
		return;
	}
	struct templeos_mem_entry_t *e = get_templeos_memory((uint64_t)p);
	
	if (DEBUG_ALLOC) {
		printf("Freeing %p (%p)", p, e);
		if (e != NULL) {
			printf(" mem=%p sz=%lx is_mmapped=%d", e->mem, e->sz, e->is_mmapped);
			if (p != e->mem) {
				printf(" MISMATCH");
			}
		}
		printf("\n");
		//print_stack_trace(stdout, t.Fs->rip, t.Fs->rbp);
	}
	
	if (e == NULL) {
		printf("trying to free unknown memory %p\n", p);
		fflush(stdout);
		_exit(0);
	}
	
	if (e->is_mmapped) {
		munmap(p, e->sz);
	} else {
		free(e->mem);
	}
	
	unregister_templeos_memory((uint64_t)p);
}

