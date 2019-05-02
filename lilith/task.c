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

#define MEM_HEAP_HASH_SIZE 1024
#define MEM_PAG_SIZE (1<<MEM_PAG_BITS)

struct CHeapCtrl {
	struct CBlkPool *bp; // 0x0
	uint32_t   hc_signature, pad; // 0x8, 0xa
	int64_t   locked_flags, alloced_u8s, used_u8s; // 0x10, 0x18, 0x20
	struct CTask *mem_task; // 0x28
	struct CMemBlk *next_mem_blk, *last_mem_blk; // 0x30, 0x38
	struct CMemBlk *last_mergable; // 0x40
	struct CMemUnused *malloc_free_lst;
	struct CMemUsed *next_um,*last_um;
	struct CMemUnused *heap_hash[MEM_HEAP_HASH_SIZE/sizeof(uint8_t *)];
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

#define DATA_HEAP_SIZE (10000 * MEM_PAG_SIZE)
#define CODE_HEAP_SIZE (10000 * MEM_PAG_SIZE)
#define TASK_HASH_TABLE_SIZE	(1<<10)

void init_templeos(struct templeos_thread *t, void *stk_base_estimate) {
	t->Gs = (struct CCPU *)calloc(1, sizeof(struct CCPU));
	t->Gs->addr = t->Gs;
	t->Fs = (struct CTask *)calloc(1, sizeof(struct CTask));
	t->Fs->addr = t->Fs;
	t->Fs->gs = t->Gs;
	t->Fs->task_signature = 0x536b7354; //TskS
	
	//TODO: make this thing alinged too
	struct CBlkPool *data_bp = (struct CBlkPool *)malloc(DATA_HEAP_SIZE * sizeof(uint8_t));
	register_templeos_memory(data_bp, DATA_HEAP_SIZE * sizeof(uint8_t));
	struct CBlkPool *code_bp = (struct CBlkPool *)malloc_executable_aligned(CODE_HEAP_SIZE * sizeof(uint8_t), MEM_PAG_SIZE, 0);
	
	// Starting from here we should do calls to actual templeos functions
	blk_pool_init(data_bp, DATA_HEAP_SIZE / MEM_PAG_SIZE);
	blk_pool_init(code_bp, CODE_HEAP_SIZE / MEM_PAG_SIZE);
	
	t->Fs->data_heap = heap_ctrl_init(data_bp, t->Fs);
	t->Fs->code_heap = heap_ctrl_init(code_bp, t->Fs);
	
	t->Fs->hash_table = templeos_hash_table_new(TASK_HASH_TABLE_SIZE);
	
	t->Fs->cur_dv = templeos_memory_calloc(sizeof(struct CDrv));
	t->Fs->cur_dv->dv_signature = 0x56535644;
	t->Fs->cur_dv->fs_type = 1; // we're going to impersonate the RedSea file system (because it's easier)
	t->Fs->cur_dv->drv_let = DRIVE_LETTER;
	t->Fs->cur_dv->bd = templeos_memory_calloc(sizeof(struct CBlkDev));
	t->Fs->cur_dv->bd->bd_signature = 0x56534442;
	t->Fs->cur_dv->bd->type = 1; // Ram disk
	
	// que_init...
	t->Fs->next_except = (struct CExcept *)(&(t->Fs->next_except));
	t->Fs->last_except = (struct CExcept *)(&(t->Fs->next_except));
	
	t->Fs->stk = templeos_memory_calloc(sizeof(struct CTaskStk));
	t->Fs->stk->stk_base = stk_base_estimate;
	t->Fs->stk->stk_size = 4 * 1024 * 1024;
	
	struct export_t *blkdev_export = hash_get(&symbols, "blkdev");
	if (blkdev_export == NULL) {
		fprintf(stderr, "Could not find global variable blkdev\n");
		exit(EXIT_FAILURE);
	}
	struct CBlkDevGlbls *blkdev = (struct CBlkDevGlbls *)(blkdev_export->val);
	blkdev->boot_drv_let = DRIVE_LETTER;
	blkdev->first_hd_drv_let = DRIVE_LETTER;
	/*char *p;
	asprintf(&p, "%c:%s", DRIVE_LETTER, (uint8_t *)getenv("HOME"));
	blkdev->home_dir = (uint8_t *)p;
	if (DEBUG_FILE_FIND) {
		printf("root directory is %s at %p\n", p, p);
	}*/
	blkdev->let_to_drv[DRIVE_LETTER - 'A'] = t->Fs->cur_dv;
	
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

void blk_pool_init(struct CBlkPool *bp, int64_t pags) {
	int64_t num;
	struct CMemBlk *m;
	memset(bp,0,sizeof(struct CBlkPool));
	m=(struct CMemBlk *)((((uint64_t)bp)+sizeof(struct CBlkPool)+MEM_PAG_SIZE-1)&~(MEM_PAG_SIZE-1)); // I'm iffy about this, this could be wrong
	num=((int64_t)bp+(pags<<MEM_PAG_BITS)-(int64_t)m)>>MEM_PAG_BITS;
	bp->alloced_u8s=(pags-num)<<MEM_PAG_BITS; //Compensate before num added.
	blk_pool_add(bp,m,num);
}

struct CMemBlk {
	struct CMemBlk *next,*last;
	uint32_t   mb_signature,pags;
};

#define MBS_USED_SIGNATURE_VAL          (uint32_t)0x7355424d
#define MBS_UNUSED_SIGNATURE_VAL        (uint32_t)0x6e55424d

void blk_pool_add(struct CBlkPool *bp, struct CMemBlk *m, int64_t pags) {
	m->next=bp->mem_free_lst;
	m->pags=pags;
	m->mb_signature=MBS_UNUSED_SIGNATURE_VAL;
	bp->alloced_u8s+=pags<<MEM_PAG_BITS;
	bp->mem_free_lst=m;
}

struct CMemUsed {
	struct CHeapCtrl *hc;
	uint8_t    *caller1,*caller2;
	struct CMemUsed *next,*last;
	int64_t   size;
};

#define HEAP_CTRL_SIGNATURE_VAL 0x56536348

struct CHeapCtrl *heap_ctrl_init(struct CBlkPool *bp, struct CTask *task) {
	struct CHeapCtrl *hc = (struct CHeapCtrl *)calloc(1, sizeof(struct CHeapCtrl));
	hc->hc_signature = HEAP_CTRL_SIGNATURE_VAL; // mov [param2+0x8], blah
	hc->mem_task = task; // mov [param2+0x28], task
	hc->bp = bp; // mov [param2] = bp
	
	// que_init...
	hc->next_mem_blk = (struct CMemBlk *)(&hc->next_mem_blk);
	hc->last_mem_blk = (struct CMemBlk *)(&hc->next_mem_blk);
	
	hc->last_mergable = NULL;
	hc->next_um=hc->last_um=(struct CMemUsed *)(((uint8_t *)(&hc->next_um))-offsetof(struct CMemUsed, next));
	return hc;
}

struct CHashTable {
  struct CHashTable *next;
  int64_t   mask,locked_flags;
  void *body;
};

struct CHashTable *templeos_hash_table_new(uint64_t size) {
	struct CHashTable *h = (struct CHashTable *)templeos_memory_calloc(sizeof(struct CHashTable));
	h->body = templeos_memory_calloc(size<<3);
	h->mask = size-1;
	return h;
}

void *templeos_memory_calloc(size_t sz) {
	void *r = malloc(sz);
	memset(r, 0, sz);
	register_templeos_memory(r, sz);
	return r;
}

void call_templeos(struct templeos_thread *t, char *name) {
	struct export_t *e = hash_get(&symbols, name);
	if (e == NULL) {
		fprintf(stderr, "Could not call %s\n", name);
		exit(EXIT_FAILURE);
	}
	void *entry = (void *)(e->val);
	fflush(stdout);
	fflush(stderr);
	
	enter_templeos(t);
	((void (*)(void))entry)();
	exit_templeos(t);
}

extern void call_templeos3_asm(void *entry, uint64_t arg1, uint64_t arg2, uint64_t arg3);

void call_templeos3(struct templeos_thread *t, char *name, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
	struct export_t *e = hash_get(&symbols, name);
	if (e == NULL) {
		fprintf(stderr, "Could not call %s\n", name);
		exit(EXIT_FAILURE);
	}
	void *entry = (void *)(e->val);
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

void putchar_c_wrapper(uint64_t c) {
	struct templeos_thread t;
	exit_templeos(&t);
	putchar(c);
	enter_templeos(&t);
}

#define CDIR_FILENAME_LEN       38

struct CDirEntry {
  struct CDirEntry *next,*parent,*sub;
  uint8_t    *full_name;
  int64_t   user_data,user_data2;

  uint16_t   attr;
  uint8_t    name[CDIR_FILENAME_LEN];
  int64_t   clus,size;
  int64_t datetime;
};

#define FUF_JUST_DIRS           0x0000400 //D
#define FUF_JUST_FILES          0x0000800 //F

#define RS_ATTR_DIR 0x10
#define RS_ATTR_COMPRESSED 0x400

int64_t redseafilefind_c_wrapper(uint64_t dv, int64_t cur_dir_clus, uint8_t *name, struct CDirEntry *_res, int64_t fuf_flags) {
	int64_t res = 0;
	struct templeos_thread t;
	exit_templeos(&t);
		
	char *dpath = "/";
	if (cur_dir_clus != 0) {
		dpath = (char *)cur_dir_clus;
	}
	
	if (DEBUG_FILE_FIND) {
		printf("RedSeaFileFind([%s] %016lx, [%s], fuf_flags=%lx)\n", dpath, cur_dir_clus, name, fuf_flags);
	}
	
	print_stack_trace(stdout, t.Fs->rip, t.Fs->rbp);
	
	DIR *d = opendir(dpath);
	struct dirent *ent = NULL;
	
	for(;;) {
		ent = readdir(d);
		if (ent == NULL) {
			break;
		}
		if ((fuf_flags & FUF_JUST_DIRS) != 0) {
			if (ent->d_type != DT_DIR) {
				continue;
			}
		}
		if ((fuf_flags & FUF_JUST_FILES) != 0) {
			if (ent->d_type == DT_DIR) {
				continue;
			}
		}
		if (strcmp(ent->d_name, (char *)name) == 0) {
			res = 1;
			break;
		}
	}
	
	if ((ent != NULL) && (_res != NULL)) {
		struct stat statbuf;
		char *p = malloc(strlen(dpath) + strlen(ent->d_name) + 2);
		strcpy(p, dpath);
		strcat(p, "/");
		strcat(p, ent->d_name);
		memset(&statbuf, 0, sizeof(struct stat));
		stat(p, &statbuf);
		
		if (ent->d_type == DT_DIR) {
			_res->attr |= RS_ATTR_DIR;
		}
		if (extension_is(ent->d_name, ".Z")) {
			_res->attr |= RS_ATTR_COMPRESSED;
		}
		strcpy((char *)(_res->name), ent->d_name);
		_res->size = statbuf.st_size;
		
		_res->clus = intern_path(p);
		_res->datetime = 0; // TODO: actually fill with the proper stuff
		
		if (DEBUG_FILE_FIND) {
			printf("\tfound %lx\n", _res->clus);
		}
		
		free(p);
	} else if (DEBUG_FILE_FIND) {
		printf("\tnot found\n");
	}
	
	printf("cur dir: %s %p\n", t.Fs->cur_dir, t.Fs->cur_dir);

	closedir(d);
	
	enter_templeos(&t);
	return res;
}
