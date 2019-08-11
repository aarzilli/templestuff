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
#define TMP_FILE_NAME "~/Tmp.DD.Z"

void task_pthread_initialize(struct templeos_thread *t) {
	pthread_mutex_init(&(t->Fs->lilith_task_mutex), NULL);
	pthread_cond_init(&(t->Fs->lilith_enqueued_cond), NULL);
	pthread_cond_init(&(t->Fs->lilith_idle_cond), NULL);
}

void init_templeos(struct templeos_thread *t, void *stk_base_estimate) {
	t->Gs = (struct CCPU *)calloc(1, sizeof(struct CCPU));
	t->Gs->addr = t->Gs;
	t->Fs = malloc_for_templeos(sizeof(struct CTask), code_heap, true);
	t->Fs = (struct CTask *)calloc(1, sizeof(struct CTask));
	
	task_pthread_initialize(t);
	
	t->Fs->addr = t->Fs;
	t->Fs->next_task = t->Fs->last_task = t->Fs->next_input_filter_task = t->Fs->last_input_filter_task = t->Fs;
	t->Fs->gs = t->Gs;
	t->Fs->task_signature = TASK_SIGNATURE;
	
	t->Fs->data_heap = (uint64_t)data_heap;
	t->Fs->code_heap = (uint64_t)code_heap;
	
	t->Fs->cur_dv = malloc_for_templeos(sizeof(struct CDrv), data_heap, true);
	t->Fs->cur_dv->dv_signature = 0x56535644;
	t->Fs->cur_dv->fs_type = 1; // we're going to impersonate the RedSea file system (because it's easier)
	t->Fs->cur_dv->drv_let = DRIVE_LETTER;
	t->Fs->cur_dv->bd = malloc_for_templeos(sizeof(struct CBlkDev), data_heap, true);
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
	char *homep = fileconcat(DRIVE_ROOT_PATH, getenv("HOME")+1, true);
	blkdev->home_dir = (uint8_t *)homep;
	blkdev->let_to_drv[DRIVE_LETTER - 'A'] = t->Fs->cur_dv;
	blkdev->tmp_filename = malloc_for_templeos(strlen(TMP_FILE_NAME)+1, data_heap, false);
	strcpy((char *)(blkdev->tmp_filename), TMP_FILE_NAME);
	
	// que_init
	t->Fs->next_except = (struct CExcept *)(&(t->Fs->next_except));
	t->Fs->last_except = (struct CExcept *)(&(t->Fs->next_except));
	
	// que_init
	t->Fs->next_cc = (struct CCmpCtrl *)(&(t->Fs->next_cc));
	t->Fs->last_cc = (struct CCmpCtrl *)(&(t->Fs->next_cc));
	
	// que_init
	t->Fs->srv_ctrl.next_waiting = (void *)&(t->Fs->srv_ctrl.next_waiting);
	t->Fs->srv_ctrl.last_waiting = (void *)&(t->Fs->srv_ctrl.next_waiting);
	
	// que_init
	t->Fs->srv_ctrl.next_done = (void *)&(t->Fs->srv_ctrl.next_done);
	t->Fs->srv_ctrl.last_done = (void *)&(t->Fs->srv_ctrl.next_done);
	
	// que_init
	t->Fs->next_ctrl = (void *)&(t->Fs->next_ctrl);
	t->Fs->last_ctrl = (void *)&(t->Fs->next_ctrl);
	
	t->Fs->stk = malloc_for_templeos(sizeof(struct CTaskStk), data_heap, true);
	t->Fs->stk->stk_base = stk_base_estimate;
	t->Fs->stk->stk_size = 4 * 1024 * 1024;
	
	strcpy((char *)t->Fs->task_name, "ADAM");
	
	if (arch_prctl(ARCH_GET_GS, (uint64_t)&(t->Gs->glibc_gs)) == -1) {
		fprintf(stderr, "could not read gs segment: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (arch_prctl(ARCH_GET_FS, (uint64_t)&(t->Gs->glibc_fs)) == -1) {
		fprintf(stderr, "could not read fs segment: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	
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
	
	struct templeos_thread_info *ti = calloc(sizeof(struct templeos_thread_info), 1);
	ti->t = *t;
	ti->next = first_templeos_task;
	first_templeos_task = ti;
}

void enter_templeos(struct templeos_thread *t) {
	if ((t->Gs == NULL) || (t->Fs == NULL)) {
		abort();
	}
	char *error_phase = "none";
	
	fflush(stdout);
	fflush(stderr);
	
	if (TEMPLEOS_ENTER_EXIT_CHECKS) {
		if (arch_prctl(ARCH_GET_GS, (uint64_t)&(t->Gs->glibc_gs)) == -1) {
			error_phase = "saving GS base";
			goto enter_templeos_failed;
			
		}
		if (arch_prctl(ARCH_GET_FS, (uint64_t)&(t->Gs->glibc_fs)) == -1) {
			error_phase = "saving FS base";
			goto enter_templeos_failed;
			
		}
	}

	if (arch_prctl(ARCH_SET_GS, (uint64_t)(t->Gs)) == -1) {
		error_phase = "set GS base";
		goto enter_templeos_failed;
	}
	if (arch_prctl(ARCH_SET_FS, (uint64_t)(t->Fs)) == -1) {
		error_phase = "set FS base";
		goto enter_templeos_failed;
	}
	
	if (TEMPLEOS_ENTER_EXIT_CHECKS) {
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

	if (TEMPLEOS_ENTER_EXIT_CHECKS) {
		if (arch_prctl(ARCH_GET_GS, (uint64_t)&(t->Gs)) == -1) {
			error_phase = "get GS base";
			goto exit_templeos_failed;
		}
		if (arch_prctl(ARCH_GET_FS, (uint64_t)&(t->Fs)) == -1) {
			error_phase = "get GS base";
			goto exit_templeos_failed;
		}
	} else {
		uint64_t templeos_gs, templeos_fs;
		// the following works because Terry made the first field of FS and GS structs be a self pointer.
		asm("movq %%gs:0, %0 \n movq %%fs:0, %1 \n" : "=r" (templeos_gs), "=r" (templeos_fs));
		t->Gs = (struct CCPU *)templeos_gs;
		t->Fs = (struct CTask *)templeos_fs;
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
		if (is_hash_type(it.he, HTT_EXPORT_SYS_SYM|HTF_IMM)) {
			if (strcmp((char *)(it.he->str), name) != 0) {
				continue;
			}
			struct CHashExport *ex = (struct CHashExport *)(it.he);
			return (void *)(ex->val);
		} else if (is_hash_type(it.he, HTT_FUN)) {
			if (strcmp((char *)(it.he->str), name) != 0) {
				continue;
			}
			struct CHashFun *fn = (struct CHashFun *)(it.he);
			return (void *)(fn->exe_addr);
		}
		
	}
	return NULL;
}

uint64_t call_templeos(struct templeos_thread *t, char *name) {
	void *entry = find_entry_point(t, name);
	if (entry == NULL) {
		fprintf(stderr, "Could not call %s\n", name);
		exit(EXIT_FAILURE);
	}
	fflush(stdout);
	fflush(stderr);
	
	enter_templeos(t);
	uint64_t r = ((uint64_t (*)(void))entry)();
	exit_templeos(t);
	return r;
}

extern uint64_t call_templeos1_asm(void *entry, uint64_t arg1);

uint64_t call_templeos1(struct templeos_thread *t, char *name, uint64_t arg1) {
	void *entry = find_entry_point(t, name);
	if (entry == NULL) {
		fprintf(stderr, "Could not call %s\n", name);
		exit(EXIT_FAILURE);
	}
	fflush(stdout);
	fflush(stderr);
	
	enter_templeos(t);
	uint64_t r = call_templeos1_asm(entry, arg1);
	exit_templeos(t);
	return r;
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

extern uint64_t call_templeos4_asm(void *entry, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4);

uint64_t call_templeos4(struct templeos_thread *t, char *name, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4) {
	void *entry = find_entry_point(t, name);
	if (entry == NULL) {
		fprintf(stderr, "Could not call %s\n", name);
		exit(EXIT_FAILURE);
	}
	fflush(stdout);
	fflush(stderr);
	
	enter_templeos(t);
	uint64_t r = call_templeos4_asm(entry, arg1, arg2, arg3, arg4);
	exit_templeos(t);
	return r;
}


// malloc_for_templeos returns a chunk of memory of the specified size
// allocated for TempleOS (TempleOS will be able to call Free on it).
void *malloc_for_templeos(uint64_t size, stbm_heap *heap, bool zero) {
	if (USE_GLIBC_MALLOC) {
		bool executable = (heap == code_heap);
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
		if (p == 0) {
			return p;
		}
		if (is_mmapped || DEBUG_REGISTER_ALL_ALLOCATIONS) {
			register_templeos_memory(p, size, is_mmapped);
		}
		return p;
	} else {
		void *p = stbm_alloc(NULL, heap, size, 0);
		if (zero) {
			memset(p, 0, size);
		}
		return p;
	}
}

// free_for_templeos frees memory allocated for TempleOS (p could have been
// allocated by calling MAlloc).
void free_for_templeos(void *p) {
	if (p == NULL) {
		return;
	}
		
	if (USE_GLIBC_MALLOC) {
		struct templeos_mem_entry_t *e = NULL;
		if (DEBUG_REGISTER_ALL_ALLOCATIONS || (((uint64_t)p) < 0x100000000)) {
			e = get_templeos_memory((uint64_t)p);
		}
		
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
		
		if ((e == NULL) && DEBUG_REGISTER_ALL_ALLOCATIONS) {
			printf("trying to free unknown memory %p\n", p);
			fflush(stdout);
			_exit(0);
		}
		
		if (DEBUG_REGISTER_ALL_ALLOCATIONS) {
			if (e->mem != p) {
				printf("mismatched free\n");
				fflush(stdout);
				_exit(0);
			}
		}
		
		if (e != NULL) {
			if (e->is_mmapped) {
				munmap(e->mem, e->sz);
			} else {
				free(e->mem);
			}
			unregister_templeos_memory((uint64_t)p);
		} else {
			free(p);
		}
	} else {
		stbm_heap *h = stbm_get_heap(p);
		stbm_free(NULL, h, p);
	}
}

pthread_mutex_t thread_create_destruct_mutex = PTHREAD_MUTEX_INITIALIZER;
struct templeos_thread_info *first_templeos_task = NULL;

void *templeos_task_start(void *arg) {
	struct templeos_thread_info *ti = (struct templeos_thread_info *)arg;
		
	if (arch_prctl(ARCH_GET_GS, (uint64_t)&(ti->t.Gs->glibc_gs)) == -1) {
		fprintf(stderr, "could not read gs segment: %s\n", strerror(errno));
		free(ti->t.Fs);
		free(ti);
		pthread_exit(NULL);
		return NULL;
	}
	if (arch_prctl(ARCH_GET_FS, (uint64_t)&(ti->t.Gs->glibc_fs)) == -1) {
		fprintf(stderr, "could not read fs segment: %s\n", strerror(errno));
		free(ti->t.Fs);
		free(ti);
		pthread_exit(NULL);
		return NULL;
	}
	
	call_templeos2(&ti->t, "TaskInit", (uint64_t)(ti->t.Fs), 0);
	ti->t.Fs->hash_table->next = ti->t.Fs->parent_task->hash_table;
	
	pthread_mutex_lock(&thread_create_destruct_mutex);
	{
		fprintf(stderr, "first task was %p\n", first_templeos_task);
		ti->next = first_templeos_task;
		first_templeos_task = ti;
		if (DEBUG_TASKS) {
			fprintf(stderr, "new task started for %p/%s\n", ti->t.Fs, ti->t.Fs->task_name);
		}
	}
	pthread_mutex_unlock(&thread_create_destruct_mutex);
	
	pthread_mutex_lock(&(ti->t.Fs->lilith_task_mutex));
	pthread_cond_broadcast(&(ti->t.Fs->lilith_idle_cond));
	pthread_mutex_unlock(&(ti->t.Fs->lilith_task_mutex));
	
	fflush(stdout);
	fflush(stderr);
	
	enter_templeos(&ti->t);
	call_templeos1_asm(ti->fp, (uint64_t)ti->data);
	exit_templeos(&ti->t);
	
	//TODO: handle task destruction (here, everywhere we call pthread_exit and also as callback to TaskEnd?
	if (DEBUG_TASKS) {
		pthread_mutex_lock(&thread_create_destruct_mutex);
		fprintf(stderr, "task finished %p/%s\n", ti->t.Fs, ti->t.Fs->task_name);
		pthread_mutex_unlock(&thread_create_destruct_mutex);
	}
	
	return NULL;
}
