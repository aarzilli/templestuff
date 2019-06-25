void syscall_RawPutChar(uint64_t c) {
	struct templeos_thread t;
	exit_templeos(&t);
	if (c != '\r') {
		putchar(c);
	} else {
		putchar('\\');
		putchar('r');
	}
	enter_templeos(&t);
}

//uint64_t syscall_DrvLock(uint64_t); NOP
//uint64_t syscall_JobsHndlr(uint64_t); NOP

void *syscall_MAlloc(uint64_t size, uint64_t mem_task) { // _MALLOC
	struct templeos_thread t;
	exit_templeos(&t);
	
	void *p = malloc_for_templeos(size, mem_task == CODE_HEAP_FAKE_POINTER, false);
	
	if (DEBUG_ALLOC) {
		printf("Allocated %p to %p (%lx)\n", p, p+size, size);
		//print_stack_trace(stdout, t.Fs->rip, t.Fs->rbp);
	}
	enter_templeos(&t);
	return p;
}

void syscall_Free(void *p) { // _FREE
	if (p == NULL) {
		return;
	}
	struct templeos_thread t;
	exit_templeos(&t);
	
	free_for_templeos(p);
	
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

#define FUF_RECURSE		0x0000001 //r
#define FUF_JUST_DIRS           0x0000400 //D
#define FUF_JUST_FILES          0x0000800 //F
#define FUF_JUST_FILES		0x0000800 //F
#define FUF_JUST_TXT		0x0001000 //T
#define FUF_JUST_DD		0x0002000 //$$
#define FUF_JUST_SRC		0x0004000 //S
#define FUF_JUST_AOT		0x0008000 //A
#define FUF_JUST_JIT		0x0010000 //J
#define FUF_JUST_GR		0x0020000 //G

#define RS_ATTR_DIR 0x10
#define RS_ATTR_COMPRESSED 0x400

void dirent_to_cdirentry(char *dir, struct dirent *ent, struct CDirEntry *_res) {
	char *p = fileconcat(dir, ent->d_name, false);
	
	struct stat statbuf;
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
	
	free(p);
}

int64_t filefind(char *dpath, char *name, struct CDirEntry *_res, int64_t fuf_flags) {
	DIR *d = opendir(dpath);
	struct dirent *ent = NULL;
	uint64_t res = 0;
	
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
		if (strcmp(ent->d_name, name) == 0) {
			res = 1;
			break;
		}
	}
	
	if ((ent != NULL) && (_res != NULL)) {
		dirent_to_cdirentry(dpath, ent, _res);
		
		if (DEBUG_FILE_SYSTEM) {
			printf("\tfound %lx\n", _res->clus);
		}
		
	} else if (DEBUG_FILE_SYSTEM) {
		printf("\tnot found\n");
	}

	closedir(d);
	
	return res;
}

void builtin_file_to_cdirentry(struct builtin_file *builtin_file, struct CDirEntry *_res) {
	if (extension_is(builtin_file->name, ".Z")) {
		_res->attr |= RS_ATTR_COMPRESSED;
	}
	strcpy((char *)(_res->name), builtin_file->name);
	_res->size = builtin_file->size;
	_res->datetime = 0;
	_res->clus = intern_path(fileconcat("/", builtin_file->name, false));
}

int64_t syscall_RedSeaFileFind(uint64_t dv, int64_t cur_dir_clus, uint8_t *name, struct CDirEntry *_res, int64_t fuf_flags) {
	struct templeos_thread t;
	exit_templeos(&t);
		
	char *dpath = "/";
	if (cur_dir_clus != 0) {
		dpath = (char *)cur_dir_clus;
	}
	
	if (DEBUG_FILE_SYSTEM) {
		printf("RedSeaFileFind([%s] %016lx, [%s], fuf_flags=%lx)\n", dpath, cur_dir_clus, name, fuf_flags);
		//print_stack_trace(stdout, t.Fs->rip, t.Fs->rbp);
	}
	
	if (strcmp(dpath, "/") == 0) {
		if ((fuf_flags & FUF_JUST_DIRS) == 0){
			for (int i = 0; i < NUM_BUILTIN_FILES; ++i) {
				if (strcmp((char *)name, builtin_files[i].name) == 0) {
					if (_res != NULL) {
						builtin_file_to_cdirentry(&builtin_files[i], _res);
					}
					if (DEBUG_FILE_SYSTEM) {
						printf("\tfound (%s)\n", builtin_files[i].name);
					}
					enter_templeos(&t);
					return 1;
				}
			}
		}
		
		if (templeos_root != NULL) {
			char *p = fileconcat(templeos_root, dpath+1, false);
			uint64_t res = filefind(p, (char *)name, _res, fuf_flags);
			free(p);
			if (res != 0) {
				enter_templeos(&t);
				return res;
			}
		}
	}
	
	uint64_t res = filefind(dpath, (char *)name, _res, fuf_flags);
	enter_templeos(&t);
	return res;
}

char *fileread(char *p, int64_t *psize, int64_t *pattr) {
	struct stat statbuf;
	memset(&statbuf, 0, sizeof(struct stat));
	if (stat(p, &statbuf) != 0) {
		return NULL;
	}
	if (pattr != NULL) {
		*pattr = 0;
		if (extension_is(p, ".Z")) {
			*pattr |= RS_ATTR_COMPRESSED;
		}
		if ((statbuf.st_mode & S_IFMT) == S_IFDIR) {
			*pattr |= RS_ATTR_DIR;
		}
	}
	
	if (psize != NULL) {
		*psize = statbuf.st_size;
	}
	
	int fd = open(p, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "could not open %s: %s\n", p, strerror(errno));
		return NULL;
	}
	char *buf = (char *)malloc_for_templeos(statbuf.st_size+1, false, false);
	char *rdbuf = buf;
	int toread = statbuf.st_size;
	
	while(toread > 0) {
		int n = read(fd, rdbuf, toread);
		if (n == 0) {
			break;
		}
		if (n < 0) {
			if (errno == EAGAIN) {
				continue;
			}
			fprintf(stderr, "error while reading %s: %s\n", p, strerror(errno));
			free_for_templeos(buf);
			return NULL;
		}
		
		rdbuf += n;
		toread -= n;
	}
	
	buf[statbuf.st_size] = '\0';
	
	close(fd);
	
	return buf;
}

char *syscall_RedSeaFileRead(uint64_t cdrv, char *cur_dir, char *filename, int64_t *psize, int64_t *pattr) {
	struct templeos_thread t;
	exit_templeos(&t);
	
	if (DEBUG_FILE_SYSTEM) {
		printf("RedSeaFileRead([%s], [%s], %p, %p)\n", cur_dir, filename, psize, pattr);
		//print_stack_trace(stdout, t.Fs->rip, t.Fs->rbp);
	}
	
	if (strcmp(cur_dir, "/") == 0) {
		for (int i = 0; i < NUM_BUILTIN_FILES; ++i) {
			if (strcmp(filename, builtin_files[i].name) == 0) {
				if (DEBUG_FILE_SYSTEM) {
					printf("\tCompiler.BIN.Z built in\n");
				}
				if (pattr != NULL) {
					*pattr = 0;
					if (extension_is(filename, ".Z")) {
						*pattr |= RS_ATTR_COMPRESSED;
					}
				}
				if (psize != NULL) {
					*psize = builtin_files[i].size;
				}
				char *buf = malloc_for_templeos(builtin_files[i].size, false, false);
				fflush(stdout);
				memcpy(buf, builtin_files[i].body, builtin_files[i].size);
				enter_templeos(&t);
				return buf;
			}
		}
	}
	
	if ((templeos_root != NULL) && (strlen(cur_dir) > 0) && (cur_dir[0] == '/')) {
		char *dp = fileconcat(templeos_root, cur_dir+1, false);
		char *p = fileconcat(dp, filename, false);
		free(dp);
		char *buf = fileread(p, psize, pattr);
		free(p);
		if (buf != NULL) {
			enter_templeos(&t);
			return buf;
		}
	}
	
	char *p = fileconcat(cur_dir, filename, false);
	char *buf = fileread(p, psize, pattr);
	free(p);
	
	enter_templeos(&t);
	return buf;
}

bool wildmatch(char *test_str, char *wild_str) {
	char *fall_back_src = NULL, *fall_back_wild = NULL;
	for (;;) {
		char ch1 = *test_str++;
		if (ch1 == 0) {
			return ((*wild_str == 0) || (*wild_str == '*'));
		}
		
		char ch2 = *wild_str++;
		if (ch2 == 0) {
			return false;
		}
		
		if (ch2 == '(') {
			fall_back_wild = wild_str - 1;
			fall_back_src = test_str;
			ch2 = *wild_str++;
			if (ch2 == 0) {
				return true;
			}
			while (ch2 != ch1) {
				ch1 = *test_str++;
				if (ch1 == 0) {
					return false;
				}
			}
		} else if ((ch2 != '?') && (ch1 != ch2)) {
			if (fall_back_wild) {
				wild_str = fall_back_wild;
				test_str = fall_back_src;
				fall_back_wild = NULL;
				fall_back_src = NULL;
			} else {
				return false;
			}
		}
	}
}

#define FILEMASK_JIT	"*.HC*;*.HH*"
#define FILEMASK_AOT	"*.HC*;*.HH*;*.PRJ*"
#define FILEMASK_SRC	"*.HC*;*.HH*;*.IN*;*.PRJ*"
#define FILEMASK_DD	FILEMASK_SRC ";*.DD*"
#define FILEMASK_TXT	FILEMASK_DD ";*.TXT*"
#define FILEMASK_GR	"*.GR*"

bool filesfindmatch(char *full_name, char *files_find_mask, int64_t fuf_flags) {
	char *base_name = rindex(full_name, '/');
	if (base_name != NULL) {
		++base_name;
	}
	
	bool res = false;
	
	char *mask = strclone(files_find_mask);
	char *saveptr;
		
	for (;;) {
		char *mask2 = strtok_r(mask, ";", &saveptr);
		if (mask != NULL) {
			free(mask);
			mask = NULL;
		}
		if (mask2 == NULL) {
			break;
		}
		char *p = full_name;
		if ((index(mask2, '/') >= 0) && (base_name != NULL)) {
			p = base_name;
		}
		if (*mask2 == '!') {
			if (wildmatch(p, mask2+1)) {
				return false;
			}
		} else {
			if (wildmatch(p, mask2)) {
				if (((fuf_flags&FUF_JUST_TXT) != 0) && !filesfindmatch(full_name, FILEMASK_TXT, 0)) {
					return false;
				} else if (((fuf_flags&FUF_JUST_DD) != 0) && !filesfindmatch(full_name, FILEMASK_DD, 0)) {
					return false;
				} else if (((fuf_flags&FUF_JUST_SRC) != 0) && !filesfindmatch(full_name, FILEMASK_SRC, 0)) {
					return false;
				} else if (((fuf_flags&FUF_JUST_AOT) != 0) && !filesfindmatch(full_name, FILEMASK_AOT, 0)) {
					return false;
				} else if (((fuf_flags&FUF_JUST_JIT) != 0) && !filesfindmatch(full_name, FILEMASK_JIT, 0)) {
					return false;
				} else if (((fuf_flags&FUF_JUST_GR) != 0) && !filesfindmatch(full_name, FILEMASK_GR, 0)) {
					return false;
				} else {
					res = true;
				}
			}
		}
	}
	return res;
}

struct CDirEntry *filesfind(char *dir, int ignore, char *files_find_mask, int64_t fuf_flags, struct CDirEntry *parent, struct CDirEntry *res) {
	DIR *d = opendir(dir);
	if (d == NULL) {
		return res;
	}
	
	char *dir_with_drive = fileconcat(DRIVE_ROOT_PATH, dir+ignore, false);
	
	for(;;) {
		struct dirent *ent = readdir(d);
		if (ent == NULL) {
			break;
		}
		
		char *full_name = fileconcat(dir_with_drive, ent->d_name, true);
		bool full_name_used = false;
		
		if (((fuf_flags&FUF_RECURSE) != 0) && (ent->d_type == DT_DIR) && (ent->d_name[0] != '.')) {
			struct CDirEntry *tempde = (struct CDirEntry *)malloc_for_templeos(sizeof(struct CDirEntry), false, true);
			dirent_to_cdirentry(dir, ent, tempde);
			
			tempde->parent = parent;
			tempde->next = res;
			res = tempde;
			tempde->full_name = (uint8_t *)full_name;
			full_name_used = true;
			
			char *p = fileconcat(dir, ent->d_name, false);
			tempde->sub = filesfind(p, ignore, files_find_mask, fuf_flags, tempde, NULL);
			free(p);
		} else {
			// is this what madness^Wgenius looks like?
			if (((ent->d_type == DT_DIR) || 
				((fuf_flags&FUF_JUST_DIRS) == 0)) && 
				!(((fuf_flags&FUF_RECURSE) != 0) && (ent->d_name[0] != '.') && 
				(ent->d_type == DT_DIR)) && 
				filesfindmatch(full_name, files_find_mask, fuf_flags)) {
				
				struct CDirEntry *tempde = (struct CDirEntry *)malloc_for_templeos(sizeof(struct CDirEntry), false, true);
				dirent_to_cdirentry(dir, ent, tempde);
				
				tempde->parent = parent;
				tempde->next = res;
				res = tempde;
				tempde->full_name = (uint8_t *)full_name;
				full_name_used = true;
			}
		}
		
		if (!full_name_used) {
			free_for_templeos(full_name);
		}
	}
	
	free(dir_with_drive);
	closedir(d);
	
	return res;
}

uint64_t syscall_RedSeaFilesFind(char *files_find_mask, int64_t fuf_flags, struct CDirEntry *parent) {
	struct CDirEntry *res = NULL;
	
	struct templeos_thread t;
	exit_templeos(&t);
	
	printf("RedSeaFilesFind %s %s\n", (char *)(t.Fs->cur_dir), files_find_mask);
	
	if (strcmp((char *)(t.Fs->cur_dir), "/") == 0) {
		if ((fuf_flags & FUF_JUST_DIRS) == 0){
			for (int i = 0; i < NUM_BUILTIN_FILES; ++i) {
				char *full_name = fileconcat(DRIVE_ROOT_PATH, builtin_files[i].name, true);
				if (!filesfindmatch(full_name, files_find_mask, fuf_flags)) {
					free_for_templeos(full_name);
					continue;
				}
				
				struct CDirEntry *tempde = (struct CDirEntry *)malloc_for_templeos(sizeof(struct CDirEntry), false, true);
				builtin_file_to_cdirentry(&builtin_files[i], tempde);
				
				tempde->parent = parent;
				tempde->next = res;
				res = tempde;
				tempde->full_name = (uint8_t *)full_name;
			}
		}
		
		if (templeos_root != NULL) {
			res = filesfind(templeos_root, strlen(templeos_root), files_find_mask, fuf_flags, parent, res);
		}
		
		res = filesfind("/", 0, files_find_mask, fuf_flags, parent, res);
	} else {
		bool done = false;
		if (templeos_root != NULL) {
			struct stat statbuf;
			char *p = fileconcat(templeos_root, (char *)t.Fs->cur_dir, false);
			memset(&statbuf, 0, sizeof(struct stat));
			if (stat(p, &statbuf) == 0) {
				done = true;
				res = filesfind(p, strlen(templeos_root), files_find_mask, fuf_flags, parent, res);
			}
		}
		
		if (!done) {
			res = filesfind((char *)t.Fs->cur_dir, 0, files_find_mask, fuf_flags, parent, res);
		}
	}
	
	enter_templeos(&t);
	return (uint64_t)res;
}

uint64_t syscall_SysTimerRead(void) {
	struct templeos_thread t;
	exit_templeos(&t);
	
	struct timespec tv;
	clock_gettime(CLOCK_MONOTONIC, &tv);
	
	uint64_t r = tv.tv_nsec / 832;
	r += tv.tv_sec * 1201471;
	
	enter_templeos(&t);
	
	return r;
}

//uint64_t syscall_Snd(int8_t); NOP

uint64_t syscall_MHeapCtrl(uint8_t *p) { // _MHEAP_CTRL
	struct templeos_thread t;
	exit_templeos(&t);
	
	struct templeos_mem_entry_t *e = NULL;
	if (DEBUG_REGISTER_ALL_ALLOCATIONS || (((uint64_t)p) < 0x100000000)) {
		e = get_templeos_memory((uint64_t)p);
	}
	
	uint64_t r;
	if ((e != NULL) && e->is_mmapped) {
		r = CODE_HEAP_FAKE_POINTER;
	} else {
		r = DATA_HEAP_FAKE_POINTER;
	}
	
	// return heap ctrl for this location?
	
	enter_templeos(&t);
	return r;
}

struct CDateStruct
{
  uint8_t	sec10000,sec100,sec,min,hour,
	day_of_week,day_of_mon,mon;
  int32_t	year;
};

void syscall_NowDateTimeStruct(struct CDateStruct *_ds) {
	struct templeos_thread t;
	exit_templeos(&t);
	
	memset(_ds, 0, sizeof(struct CDateStruct));
	
	struct tm tm;
	memset(&tm, 0, sizeof(struct tm));
	
	time_t now = time(NULL);
	localtime_r(&now, &tm);
	
	_ds->year = tm.tm_year + 1900;
	_ds->mon = tm.tm_mon + 1;
	_ds->day_of_mon = tm.tm_mday;
	_ds->day_of_week = tm.tm_wday;
	_ds->hour = tm.tm_hour;
	_ds->min = tm.tm_min;
	_ds->sec = tm.tm_sec;
	
	enter_templeos(&t);
}

uint64_t syscall_GetS(char *buf, int64_t size, uint8_t allow_ext) {
	struct templeos_thread t;
	exit_templeos(&t);
	
	char *r = fgets(buf, size, stdin);
	if (r == NULL) {
		buf[0] = 0;
	}
	if (!allow_ext && (strlen(buf) > 0) && (buf[strlen(buf)-1] == '\n')) {
		buf[strlen(buf)-1] = 0;
	}
	
	enter_templeos(&t);
	return strlen((char *)buf);
}
