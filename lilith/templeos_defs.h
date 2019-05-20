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
