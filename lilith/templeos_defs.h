#define TASK_SIGNATURE 0x536b7354  //TskS

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

struct CDC;

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
	
	pthread_mutex_t lilith_task_mutex;
	pthread_cond_t lilith_enqueued_cond;
	pthread_cond_t lilith_idle_cond;
};

struct CGrGlbls {
  int64_t	*to_8_bits,*to_8_colors;
  struct CDC	*scrn_image,	//Read only.
	*dc,		//Persistent
	*dc1,
	*dc2,		//Updated every refresh
	*dc_cache,
	*zoomed_dc;
  uint32_t	*text_base;	//See $LK,"TextBase Layer",A="HI:TextBase Layer"$. (Similar to 0xB8000 but 32 bits)
  uint16_t	*win_z_buf;

  #define SPHT_ELEM_CODE	1
  struct CHashTable *sprite_hash;

  uint16_t	*win_uncovered_bitmap;
  int64_t	highest_uncovered;
  uint16_t	*vga_text_cache;
  int64_t	pan_text_x,pan_text_y;	//[-7,7]
  void	(*fp_final_scrn_update)(struct CDC *dc);//Mouse cursor is handled here.
  void	(*fp_wall_paper)(struct CTask *task);
  void	(*fp_draw_ms)(struct CDC *dc,int64_t x,int64_t y);
  void	(*fp_draw_grab_ms)(struct CDC *dc,int64_t x,int64_t y,bool closed);
  uint8_t	*empty_sprite; //Gets assigned $LK,"gr.empty_sprite",A="FF:::/Adam/AMouse.HC,empty_sprite"$

  #define GR_PEN_BRUSHES_NUM 64
  struct CDC	*pen_brushes[GR_PEN_BRUSHES_NUM],
	*collision_pen_brushes[GR_PEN_BRUSHES_NUM],
	*even_pen_brushes[GR_PEN_BRUSHES_NUM],
	*odd_pen_brushes[GR_PEN_BRUSHES_NUM];
  int8_t	circle_lo[GR_PEN_BRUSHES_NUM][GR_PEN_BRUSHES_NUM],
	circle_hi[GR_PEN_BRUSHES_NUM][GR_PEN_BRUSHES_NUM];

  #define GR_SCRN_ZOOM_MAX	8
  uint8_t	*scrn_zoom_tables[GR_SCRN_ZOOM_MAX+1];
  int64_t	scrn_zoom,sx,sy;

  //When zoomed, this keeps the mouse centered.
  bool	continuous_scroll,
	hide_row,hide_col;
};

#define COLORS_NUM		16

typedef uint64_t CBGR48;
typedef uint32_t CColorROPU32;

struct CD3I32 {
	int32_t x, y, z;
};

struct CGrSym {
  int32_t	sx,sy,sz,pad;
//Normal of symmetry plane
  int64_t	snx,sny,snz;
};



struct CDC {
  uint64_t cdate;
  int32_t	x0,y0,
	width,width_internal,
	height,
	flags;
  CBGR48 palette[COLORS_NUM];

  //public (Change directly)
  CColorROPU32 color,
	bkcolor, //Set for use with $LK,"ROP_COLLISION",A="MN:ROP_COLLISION"$
	color2; //Internally used for $LK,"GrFloodFill",A="MN:GrFloodFill"$()
  struct CD3I32 ls; //Light source (should be normalized to 65536).

  //dither_probability_u16 is basically a U16.
  //It is activated by $LK,"ROPF_PROBABILITY_DITHER",A="MN:ROPF_PROBABILITY_DITHER"$.
  //0x0000 =100% color.c0
  //0x8000 =50%  color.c0   50% color.c1
  //0x10000=100% color.c1
  //See $LK,"::/Demo/Graphics/SunMoon.HC"$ and	$LK,"::/Demo/Graphics/Shading.HC"$.
  uint64_t dither_probability_u16;

  struct CDC *brush;

  //Set with $LK,"DCMat4x4Set",A="MN:DCMat4x4Set"$().  $LK,"Free",A="MN:Free"$() before setting.
  int64_t	*r, //rotation matrix of quads decimal in lo
	r_norm; //shifted 32 bits.  Used for scaling thick

  //public (Change directly)
  int32_t	x,y,z,
	thick;

  //Can be changed from the default $LK,"DCTransform",A="MN:DCTransform"$()
  void	(*transform)(struct CDC *dc,int64_t *x,int64_t *y,int64_t *z);

  //Can be changed from the default $LK,"DCLighting",A="MN:DCLighting"$()
  void	(*lighting)(struct CDC *dc,struct CD3I32 *p1,struct CD3I32 *p2,
	struct CD3I32 *p3,CColorROPU32 color);

  //Set by $LK,"DCSymmetrySet",A="MN:DCSymmetrySet"$() or $LK,"DCSymmetry3Set",A="MN:DCSymmetry3Set"$()
  struct CGrSym sym;

  int32_t	cur_x,cur_y,cur_z,pad;
  int64_t	collision_cnt;

  int64_t	nearest_dist,
	min_x,max_x,min_y,max_y; //Set by $LK,"DCF_RECORD_EXTENTS",A="MN:DCF_RECORD_EXTENTS"$ (scrn coordinates)

  uint32_t	dc_signature,pad2;
  struct CTask	*mem_task,*win_task;
  struct CDC	*alias;
  uint8_t	*body;

  //Set by $LK,"DCDepthBufAlloc",A="MN:DCDepthBufAlloc"$()
  int32_t	*depth_buf;
  int64_t	db_z; //private
};

#define TTS_TASK_NAME		2
#define DISPLAY_SHOW 1

struct CTextGlbls {
  int64_t	raw_col,raw_flags;
  uint8_t	*raw_scrn_image;
  int64_t	rows,cols;		//Use TEXT_ROWS,TEXT_COLS
  uint64_t	*font,*aux_font;
  uint8_t	*vga_alias,*vga_text_alias;
  uint8_t	border_chars[16];
};

#define MSG_KEY_DOWN		2
#define MSG_KEY_UP		3

#define SCF_E0_PREFIX	(1<<7)
#define SCF_KEY_UP	(1<<8)
#define SCF_SHIFT	(1<<9)
#define SCF_CTRL	(1<<10)
#define SCF_ALT		(1<<11)
#define SCF_CAPS	(1<<12)
#define SCF_NUM		(1<<13)
#define SCF_SCROLL	(1<<14)
#define SCF_NEW_KEY	(1<<15)
#define SCF_MS_L_DOWN	(1<<16)
#define SCF_MS_R_DOWN	(1<<17)
#define SCF_DELETE	(1<<18)
#define SCF_INS 	(1<<19)
#define SCF_NO_SHIFT	(1<<30)
#define SCF_KEY_DESC	(1<<31)

#define SC_ESC		0x01
#define SC_BACKSPACE	0x0E
#define SC_TAB		0x0F
#define SC_ENTER	0x1C
#define SC_SHIFT	0x2A
#define SC_CTRL		0x1D
#define SC_ALT		0x38
#define SC_CAPS		0x3A
#define SC_NUM		0x45
#define SC_SCROLL	0x46
#define SC_CURSOR_UP	0x48
#define SC_CURSOR_DOWN	0x50
#define SC_CURSOR_LEFT	0x4B
#define SC_CURSOR_RIGHT 0x4D
#define SC_PAGE_UP	0x49
#define SC_PAGE_DOWN	0x51
#define SC_HOME		0x47
#define SC_END		0x4F
#define SC_INS		0x52
#define SC_DELETE	0x53
#define SC_F1		0x3B
#define SC_F2		0x3C
#define SC_F3		0x3D
#define SC_F4		0x3E
#define SC_F5		0x3F
#define SC_F6		0x40
#define SC_F7		0x41
#define SC_F8		0x42
#define SC_F9		0x43
#define SC_F10		0x44
#define SC_F11		0x57
#define SC_F12		0x58
#define SC_PAUSE	0x61
#define SC_GUI		0xDB
#define SC_PRTSCRN1	0xAA
#define SC_PRTSCRN2	0xB7
