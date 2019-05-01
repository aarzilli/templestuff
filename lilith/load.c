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
// LDF_KERNEL implies LDF_JUST_LOAD, disables unresolved symbol errors (they will be ignored)
#define LDF_KERNEL            0x10000

#define HTT_EXPORT_SYS_SYM      0x00001 //CHashExport

#define HTF_IMM                 0x08000000

struct hash_t symbols;

// LoadPass1 executes some of the relocations specified in the "patch
// table". Other relocations are executed by LoadPass2.
void load_pass1(uint8_t *patch_table, uint8_t *module_base, int64_t ld_flags, bool *pok, void **pentry) {
	if ((ld_flags&LDF_KERNEL) != 0) {
		ld_flags |= LDF_JUST_LOAD;
	}
	if (DEBUG_LOAD_PASS1) {
		printf("\n");
	}
	uint8_t *cur = patch_table;
	int count = 0;
	for (;;) {
		uint8_t etype = *cur;
		if (etype == IET_END) {
			if (DEBUG_LOAD_PASS1) {
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

		if (DEBUG_LOAD_PASS1) {
			printf("%04lx %d relocation etype=%x i=%x <%s>\n", (uint8_t *)st_ptr - patch_table - 5, count, etype, i, st_ptr);
		}

		switch (etype) {
		case IET_REL32_EXPORT: // fallthrough
		case IET_IMM32_EXPORT: // fallthrough
		case IET_REL64_EXPORT: // fallthrough
		case IET_IMM64_EXPORT: {
			struct export_t *ex = symbols_put(st_ptr, HTT_EXPORT_SYS_SYM|HTF_IMM, 0, module_base);

			if ((etype == IET_IMM32_EXPORT) || (etype == IET_IMM64_EXPORT)) {
				ex->val = i;
			} else {
				ex->val = (uint64_t)(module_base) + i;
			}

			if (DEBUG_LOAD_PASS1) {
				printf("\texport relocation for %s, value %lx\n", st_ptr, ex->val);
			}

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
			load_one_import(&cur, module_base, ld_flags, pok);
			break;

		case IET_ABS_ADDR: {
			uint32_t cnt = i;
			if (DEBUG_LOAD_PASS1) {
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
				symbols_put(st_ptr, HTT_EXPORT_SYS_SYM|HTF_IMM, (uint64_t)ptr3, 0);
			}

			int64_t cnt = i;
			if (DEBUG_LOAD_PASS1) {
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
			register_templeos_memory(ptr3, sz);

			if (*st_ptr) {
				symbols_put(st_ptr, HTT_EXPORT_SYS_SYM|HTF_IMM, (uint64_t)ptr3, 0);
			}

			int64_t cnt = i;
			if (DEBUG_LOAD_PASS1) {
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
			*pentry = module_base+i;
			if (DEBUG_LOAD_PASS1) {
				printf("\texecutable entry point %p\n", *pentry);
			}
			break;

		default:
			fflush(stdout);
			fprintf(stderr, "\tunknown relocation type\n");
			exit(EXIT_FAILURE);
		}
	}
}

void load_one_import(uint8_t **psrc, uint8_t *module_base, int64_t ld_flags, bool *pok) {
	uint8_t *src = *psrc;
	bool first = true;
	struct export_t *tmpex = NULL;

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

				tmpex = hash_get(&symbols, st_ptr);
				if ((tmpex == NULL) && ((ld_flags&LDF_KERNEL) == 0)) {
					fprintf(stderr, "Unresolved Reference: %s\n", st_ptr);
					*pok = false;
				}
				

				//TODO: TempleOS can actually deal with some symbols being loaded after the fact, maybe lilith should too?
			}
		}
		if (DEBUG_LOAD_PASS1) {
			printf("\tload_one_import at %lx relocation etype=%x i=%lx <%s>\n", (uint8_t *)st_ptr - *psrc - 5, etype, i, st_ptr);
		}

		if (tmpex) {
			void *ptr2=module_base+i;
			i = tmpex->val;
			switch (etype) {
			case IET_REL_I8:
				*((uint8_t *)ptr2) = (uint8_t)(i-(uint64_t)ptr2-1);
				break;
			case IET_IMM_U8:
				*((uint8_t *)ptr2) = i;
				break;
			case IET_REL_I16:
				*((uint16_t *)ptr2) = (uint16_t)(i-(uint64_t)ptr2-2);
				break;
			case IET_IMM_U16:
				*((uint16_t *)ptr2) = i;
				break;
			case IET_REL_I32:
				*((uint32_t *)ptr2) = (uint32_t)(i-(uint64_t)ptr2-4);
				break;
			case IET_IMM_U32:
				*((uint32_t *)ptr2) = i;
				break;
			case IET_REL_I64:
				*((uint64_t *)ptr2) = (uint64_t)(i-(uint64_t)ptr2-8);
				break;
			case IET_IMM_I64:
				*((uint64_t *)ptr2) = i;
				break;
			}
		}
	}
	*psrc = src-1;
}

off_t cli_patch_table[] = { 0x5a87, 0x550a };

void load_bin(char *path, uint64_t ld_flags, struct templeos_thread *t) {
	if ((ld_flags&LDF_KERNEL) != 0) {
		ld_flags |= LDF_JUST_LOAD;
	}
	size_t sz;
	void *mem = load_file(path, &sz);

	if (DEBUG) {
		printf("loaded at %p\n", mem);

		for (int i = 0; i < 10; i++) {
			printf("%x ", ((unsigned char*)mem)[i]);
		}
		printf("\n");
	}

	// The code that follows is adapted from Load() in D:/Kernel/KLoad.HC.Z

	if (strncmp(((char *)mem)+4, TOSB_SIGNATURE, strlen(TOSB_SIGNATURE)) != 0) {
		fflush(stdout);
		fprintf(stderr, "signature check failed on %s", path);
		exit(EXIT_FAILURE);
	}

	uint8_t module_align_bits = (uint8_t)(((char *)mem)[2]);
	uint64_t org = read_uint64(mem, 8);
	uint64_t patch_table_offset = read_uint64(mem, 16);
	uint64_t file_size = read_uint64(mem, 16+8);

	int64_t module_align = 1 << module_align_bits;

	if (DEBUG) {
		printf("module_align_bits %x (module_align %lx)\n", module_align_bits, module_align);
		printf("org %lx\n", org);
		printf("patch_table_offset %lx\n", patch_table_offset);
		printf("file_size %lx\n", file_size);
	}

	if (file_size != sz) {
		fflush(stdout);
		fprintf(stderr, "file size mismatch: on disk = %lx in file = %lx\n", sz, file_size);
		exit(EXIT_FAILURE);
	}

	if (!module_align) {
		fflush(stdout);
		fprintf(stderr, "invalid module_align_bits %x\n", module_align_bits);
		exit(EXIT_FAILURE);
	}

	if (org != INT64_MAX) {
		fflush(stdout);
		fprintf(stderr, "can not load non-PIE binaries (org = %lx)\n", org);
		exit(EXIT_FAILURE);
	}

	int64_t misalignment = module_align-TOSB_HEADER_SIZE;
	if (misalignment < 0) misalignment &= module_align-1;
	if (module_align < 16) module_align = 16;

	void *xmem = malloc_executable_aligned(sz, module_align, misalignment);
	memcpy(xmem, mem, sz);
	free(mem);
	mem = xmem;

	if (DEBUG) {
		printf("misalignment %ld (module_align %ld)\n", misalignment, module_align);
		printf("new location at %p\n", xmem);
	}
	
	if ((ld_flags & LDF_KERNEL) != 0) {
		// replaces some CLI and STI with NOPs
		//TODO: instead of having file offsets here we should do this after the
		// kernel is loaded and use offsets relative to their containing functions,
		// so that this is a bit more resilient. Also complain louder if we don't
		// find what we expect to find.
		for (int i = 0; i < sizeof(cli_patch_table)/sizeof(off_t); i++) {
			patch_instruction(mem, cli_patch_table[i], 0xfa, 0x90);
		}
		
	}

	void *module_base = (void *)((char *)mem + TOSB_HEADER_SIZE);

	if (DEBUG) {
		printf("module_base %p\n", module_base);
	}

	void *entry = NULL;
	bool ok = true;
	load_pass1((uint8_t *)(mem + patch_table_offset), module_base, ld_flags, &ok, &entry);
	if (DEBUG) {
		printf("Entry Point %p\n", entry);
	}
	if (!ok) {
		fflush(stdout);
		fprintf(stderr, "Load failed due to unresolved symbols.\n");
		exit(EXIT_FAILURE);
	}

	if (((ld_flags&LDF_JUST_LOAD) == 0) && (entry != NULL)) {
		call_templeos(entry, t);
	}
}

struct export_t *symbols_put(char *key, uint32_t type, uint64_t val, void* module_base) {
	struct export_t *ex = (struct export_t*)malloc(sizeof(struct export_t));
	ex->type = type;
	ex->val = val;
	ex->module_base = (uint64_t)module_base;
	hash_put(&symbols, strclone(key), ex);
	return ex;
}

void patch_instruction(void *mem, off_t off, uint8_t original, uint8_t replacement) {
	if (((uint8_t *)mem)[off] == original) {
		((uint8_t *)mem)[off] = replacement;
	}
}