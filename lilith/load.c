#define IET_END                 0
//reserved
#define IET_REL_I0              2 //Fictitious
#define IET_IMM_U0              3 //Fictitious
#define IET_REL_I8              4
#define IET_IMM_U8              5
#define IET_REL_I16             6
#define IET_IMM_U16             7
#define IET_REL_I32             8
#define IET_IMM_U32             9
#define IET_REL_I64             10
#define IET_IMM_I64             11
#define IEF_IMM_NOT_REL         1
//reserved
#define IET_REL32_EXPORT        16
#define IET_IMM32_EXPORT        17
#define IET_REL64_EXPORT        18 //Not implemented
#define IET_IMM64_EXPORT        19 //Not implemented
#define IET_ABS_ADDR            20
#define IET_CODE_HEAP           21 //Not really used
#define IET_ZEROED_CODE_HEAP    22 //Not really used
#define IET_DATA_HEAP           23
#define IET_ZEROED_DATA_HEAP    24 //Not really used
#define IET_MAIN                25

#define LDF_NO_ABSS		1
#define LDF_JUST_LOAD		2
#define LDF_SILENT		4

struct hash_t symbols;

// LoadPass1 executes some of the relocations specified in the "patch
// table". Other relocations are executed by LoadPass2.
void load_pass1(uint8_t *patch_table, uint8_t *module_base, int64_t ld_flags) {
	if (DEBUG) {
		printf("\n");
	}
	uint8_t *cur = patch_table;
	int count = 0;
	for (;;) {
		uint8_t etype = *cur;
		if (etype == IET_END) {
			if (DEBUG) {
				printf("first relocation pass done after %d relocations\n", count);
			}
			break;
		}
		++count;
		++cur;

		uint32_t i = *((uint32_t *)cur);
		cur += sizeof(uint32_t);

		char *st_ptr = (char *)cur;
		cur += strlen(st_ptr) + 1;

		printf("%04lx %d relocation etype=%x i=%x <%s>\n", (uint8_t *)st_ptr - patch_table - 5, count, etype, i, st_ptr);

		switch (etype) {
		case IET_REL32_EXPORT: // fallthrough
		case IET_IMM32_EXPORT: // fallthrough
		case IET_REL64_EXPORT: // fallthrough
		case IET_IMM64_EXPORT: {
			uint64_t val;
			if ((etype == IET_IMM32_EXPORT) || (etype == IET_IMM64_EXPORT)) {
				val = i;
			} else {
				val = (uint64_t)(module_base) + i;
			}

			if (DEBUG) {
				printf("\texport relocation for %s, value %lx\n", st_ptr, val);
			}
			//TODO: implement
			break;
		}

		case IET_REL_I0: // fallthrough
		case IET_IMM_U0: // fallthrough
		case IET_REL_I8: // fallthrough
		case IET_IMM_U8: // fallthrough
		case IET_REL_I16: // fallthrough
		case IET_IMM_U16: // fallthrough
		case IET_REL_I32: // fallthrough
		case IET_IMM_U32: // fallthrough
		case IET_REL_I64: // fallthrough
		case IET_IMM_I64: // fallthrough
			cur = ((uint8_t *)st_ptr)-5;
			load_one_import(&cur, module_base, ld_flags);
			break;

		case IET_ABS_ADDR: {
			uint32_t cnt = i;
			if (DEBUG) {
				printf("\tabsolute address relocation (cnt = %d)\n", cnt);
			}
			for (uint32_t j = 0; j < cnt; j++) {
				uint32_t off = *((uint32_t *)cur);
				cur += sizeof(uint32_t);
				if ((ld_flags & LDF_NO_ABSS) == 0) {
					uint32_t *ptr2 = (uint32_t *)(module_base  + off);
					*ptr2 += (uint32_t)(uint64_t)module_base;
				}
			}
			break;
		}

		case IET_CODE_HEAP: // fallthrough
		case IET_ZEROED_CODE_HEAP: {
			int32_t sz = *((int32_t *)cur);
			cur += sizeof(int32_t);
			uint8_t *ptr3 = mmap(NULL, sz, PROT_EXEC|PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT, -1, 0);

			// note that MAP_32BIT is fundamental, this will break if the ptr3 can't be represented in 32bits, if linux ever stops honoring MAP_32BIT we're going to be fucked hard.

			if (*st_ptr) {
				//TODO: add this to hash table HTT_EXPORT_SYS_SYM|HTF_IMM with val = ptr3
			}

			int64_t cnt = i;
			if (DEBUG) {
				printf("\tcode heap allocation of size %x with %ld relocations (mapped to %p)\n", sz, cnt, ptr3);
			}
			for (int64_t j = 0; j < cnt; ++j) {
				uint32_t off = *((uint32_t *)cur);
				cur += sizeof(uint32_t);
				uint32_t *ptr2 = (uint32_t *)(module_base + off);
				*ptr2 += (uint32_t)(uint64_t)ptr3;
			}

			break;
		}

		case IET_DATA_HEAP: // fallthrough
		case IET_ZEROED_DATA_HEAP: {
			int64_t sz = *((int64_t *)cur);
			cur += sizeof(int64_t);
			uint8_t *ptr3 = malloc(sz);

			if (*st_ptr) {
				//TODO: add this to hash table HTT_EXPORT_SYS_SYM|HTF_IMM with val = ptr3
			}

			int64_t cnt = i;
			if (DEBUG) {
				printf("\tdata heap allocation of size %lx with %ld relocations (mapped to %p)\n", sz, cnt, ptr3);
			}
			for (int64_t j = 0; j < cnt; ++j) {
				uint32_t off = *((uint32_t *)cur);
				cur += sizeof(uint32_t);
				uint64_t *ptr2 = (uint64_t *)(module_base + off);
				*ptr2 += (uint64_t)ptr3;
			}

			break;
		}

		case IET_MAIN:
			if (DEBUG) {
				printf("\trelocation type unhandled in this pass\n");
			}
			break;

		default:
			fflush(stdout);
			fprintf(stderr, "\tunknown relocation type\n");
			exit(EXIT_FAILURE);
		}
	}
}

void load_one_import(uint8_t **psrc, uint8_t *module_base, int64_t ld_flags) {
	uint8_t *src = *psrc;
	bool first = true;

	for(;;) {
		uint8_t etype = *src;
		if (etype == 0) {
			break;
		}
		++src;
		int64_t i = *((int32_t *)src);
		src += sizeof(int32_t);
		char *st_ptr = (char *)src;
		src += strlen(st_ptr)+1;

		if (*st_ptr) {
			if (!first) {
				*psrc=((uint8_t *)st_ptr)-5;
				return;
			} else {
				first=false;

				//TODO: implement

				/*
				tmpex=HashFind(st_ptr, Fs->hash_table,HTG_ALL-HTT_IMPORT_SYS_SYM)
				if (!tmpex) {
					if (!(ld_flags & LDF_SILENT))
  						"Unresolved Reference:%s\n",st_ptr;
					tmpiss=CAlloc(sizeof(CHashImport));
					tmpiss->str=StrNew(st_ptr);
					tmpiss->type=HTT_IMPORT_SYS_SYM;
					tmpiss->module_header_entry=st_ptr-5;
					tmpiss->module_base=module_base;
					HashAdd(tmpiss,Fs->hash_table);
				}
				*/
			}
		}
		if (DEBUG) {
			printf("\tload_one_import at %lx relocation etype=%x i=%lx <%s>\n", (uint8_t *)st_ptr - *psrc - 5, etype, i, st_ptr);
		}

		//TODO: implement
		/*
		if (tmpex) {
			void *ptr2=module_base+i;
			if (tmpex->type & HTT_FUN) {
				i=tmpex(CHashFun *)->exe_addr;
			} else if (tmpex->type & HTT_GLBL_VAR) {
				i=tmpex(CHashGlblVar *)->data_addr;
			} else {
				i=tmpex->val;
			}
			switch (etype) {
			case IET_REL_I8:  *ptr2(U8 *) =i-ptr2-1; break;
			case IET_IMM_U8:  *ptr2(U8 *) =i;  break;
			case IET_REL_I16: *ptr2(U16 *)=i-ptr2-2; break;
			case IET_IMM_U16: *ptr2(U16 *)=i; break;
			case IET_REL_I32: *ptr2(U32 *)=i-ptr2-4; break;
			case IET_IMM_U32: *ptr2(U32 *)=i; break;
			case IET_REL_I64: *ptr2(I64 *)=i-ptr2-8; break;
			case IET_IMM_I64: *ptr2(I64 *)=i; break;
			}
		}
		*/
	}
	*psrc = src-1;
}
