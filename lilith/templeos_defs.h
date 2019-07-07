#define HTT_FUN			0x00040 //CHashFun
#define HTT_MODULE		0x04000 //CHashGeneric
#define HTT_EXPORT_SYS_SYM    0x00001 //CHashExport
#define HTT_IMPORT_SYS_SYM	0x00002 //CHashImport
#define HTT_DEFINE_STR		0x00004 //CHashDefineStr
#define HTT_GLBL_VAR		0x00008 //CHashGlblVar

#define HTF_PRIVATE		0x00800000
#define HTF_PUBLIC		0x01000000
#define HTF_EXPORT		0x02000000
#define HTF_IMPORT		0x04000000
#define HTF_IMM			0x08000000
#define HTF_GOTO_LABEL		0x10000000
#define HTF_RESOLVE		0x20000000
#define HTF_UNRESOLVED		0x40000000
#define HTF_LOCAL		0x80000000

struct CHashTable {
  struct CHashTable *next;
  int64_t   mask,locked_flags;
  struct CHash **body;
};

struct CHash {
  struct CHash	*next;
  uint8_t	*str;
  uint32_t	type,
	use_cnt; 
};

struct CHashSrcSym {
	struct CHash super;
	uint8_t	*src_link, *idx;
	struct CDbgInfo *dbg_info;
	uint8_t	*import_name;
	struct CAOTImportExport *ie_lst;
};

struct CHashExport {
	struct CHashSrcSym super;
	int64_t	val;
};

struct CHashGeneric {
	struct CHash super;
	int64_t user_data0, user_data1, user_data2;
};

struct CHashClass {
  struct CHashSrcSym super;
  int64_t	size,neg_offset;
  uint32_t	member_cnt;
  uint8_t	ptr_stars_cnt,raw_type;
  uint16_t	flags;
  struct CMemberLst *member_lst_and_root, //Head of linked list and head of tree.
	*member_class_base_root, //For finding dup class local vars.
	*last_in_member_lst;
  struct CHashClass	*base_class,
	*fwd_class;
};

struct CHashFun {
  struct CHashClass super;
  struct CHashClass *return_class;
  uint32_t	arg_cnt,pad,
	used_reg_mask,clobbered_reg_mask;
  uint8_t	*exe_addr;
  struct CExternUsage *ext_lst;
};

struct CArrayDim {
	struct CArrayDim *next;
	int64_t	cnt,total_cnt;
	
};

struct CHashGlblVar {
	struct CHashSrcSym super;
	int64_t	size,flags;
	struct CHashClass *var_class;
	struct CHashFun *fun_ptr;
	struct CArrayDim dim;
	uint8_t	*data_addr;
	union {
	  int64_t	data_addr_rip;
	  struct CAOTHeapGlbl *heap_glbl;
	};
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
