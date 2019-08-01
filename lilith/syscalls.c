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
//uint64_t syscall_KbdTypeMatic(uint64_t); NOP

void *syscall_MAlloc(uint64_t size, uint64_t mem_task) { // _MALLOC
	stbm_heap *heap = (stbm_heap*)mem_task;
	if ((heap == NULL) || ((heap != code_heap) && (heap != data_heap))) {
		heap = data_heap;
	}
	
	if (USE_GLIBC_MALLOC) {
		struct templeos_thread t;
		exit_templeos(&t);
			
		void *p = malloc_for_templeos(size, heap, false);
		
		if (DEBUG_ALLOC) {
			printf("Allocated %p to %p (%lx)\n", p, p+size, size);
			//print_stack_trace(stdout, t.Fs->rip, t.Fs->rbp);
		}
		enter_templeos(&t);
		return p;
	} else {
		return malloc_for_templeos(size, heap, false);
	}
}

void syscall_Free(void *p) { // _FREE
	if (p == NULL) {
		return;
	}
	
	if (USE_GLIBC_MALLOC) {
		struct templeos_thread t;
		exit_templeos(&t);
		
		free_for_templeos(p);
		
		enter_templeos(&t);
	} else {
		free_for_templeos(p);
	}
}

#define CDIR_FILENAME_LEN       38

struct CDirEntry {
  struct CDirEntry *next,*parent,*sub;
  uint8_t    *full_name;
  int64_t   user_data,user_data2;

  uint16_t   attr;
  uint8_t    name[CDIR_FILENAME_LEN];
  int64_t   clus,size;
  uint32_t time;
  int32_t date;
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

int64_t yearstartdate(int64_t year) {
	int64_t y1=year-1,yd4000=y1/4000,yd400=y1/400,yd100=y1/100,yd4=y1/4;
	return year*365+yd4-yd100+yd400-yd4000;
}

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
	strncpy((char *)(_res->name), ent->d_name, CDIR_FILENAME_LEN);
	_res->name[CDIR_FILENAME_LEN-1] = 0;
	_res->size = statbuf.st_size;
	
	_res->clus = intern_path(p);
	
	struct tm tm;
	memset(&tm, 0, sizeof(struct tm));
	localtime_r(&statbuf.st_mtim.tv_sec, &tm);
	
	// See Struct2Date I don't know where the constants come from and what they represent
	_res->time = ((int64_t)(10000 * (tm.tm_sec + 60 * (tm.tm_min + 60*tm.tm_hour)))<<21) / (15*15*3*625);
	_res->date = yearstartdate(tm.tm_year) + tm.tm_yday;
	
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
	_res->time = 0;
	_res->date = 0;
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
	char *buf = (char *)malloc_for_templeos(statbuf.st_size+1, data_heap, false);
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
				char *buf = malloc_for_templeos(builtin_files[i].size, data_heap, false);
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
		
		if (ch2 == '*') {
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
		
	for (char *mask2 = strtok_r(mask, ";", &saveptr); mask2 != NULL; mask2 = strtok_r(NULL, ";", &saveptr)) {
		char *p = full_name;
		if ((index(mask2, '/') >= 0) && (base_name != NULL)) {
			p = base_name;
		}
		if (*mask2 == '!') {
			if (wildmatch(p, mask2+1)) {
				res = false;
				break;
			}
		} else {
			if (wildmatch(p, mask2)) {
				if (((fuf_flags&FUF_JUST_TXT) != 0) && !filesfindmatch(full_name, FILEMASK_TXT, 0)) {
					res = false;
					break;
				} else if (((fuf_flags&FUF_JUST_DD) != 0) && !filesfindmatch(full_name, FILEMASK_DD, 0)) {
					res = false;
					break;
				} else if (((fuf_flags&FUF_JUST_SRC) != 0) && !filesfindmatch(full_name, FILEMASK_SRC, 0)) {
					res = false;
					break;
				} else if (((fuf_flags&FUF_JUST_AOT) != 0) && !filesfindmatch(full_name, FILEMASK_AOT, 0)) {
					res = false;
					break;
				} else if (((fuf_flags&FUF_JUST_JIT) != 0) && !filesfindmatch(full_name, FILEMASK_JIT, 0)) {
					res = false;
					break;
				} else if (((fuf_flags&FUF_JUST_GR) != 0) && !filesfindmatch(full_name, FILEMASK_GR, 0)) {
					res = false;
					break;
				} else {
					res = true;
				}
			}
		}
	}
	free(mask);
	return res;
}

struct CDirEntry *filesfind(char *dir, int ignore, char *files_find_mask, int64_t fuf_flags, struct CDirEntry *parent, struct CDirEntry *res) {
	DIR *d = opendir(dir);
	if (d == NULL) {
		printf("unopenable\n");
		return res;
	}
	
	char *dir_with_drive = fileconcat(DRIVE_ROOT_PATH, dir+ignore+1, false);
	
	for(;;) {
		struct dirent *ent = readdir(d);
		if (ent == NULL) {
			break;
		}
		if ((strcmp(ent->d_name, ".") == 0) || (strcmp(ent->d_name, "..") == 0)) {
			continue;
		}
		
		char *full_name = fileconcat(dir_with_drive, ent->d_name, true);
		bool full_name_used = false;
		
		
		if (((fuf_flags&FUF_RECURSE) != 0) && (ent->d_type == DT_DIR)) {
			struct CDirEntry *tempde = (struct CDirEntry *)malloc_for_templeos(sizeof(struct CDirEntry), data_heap, true);
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
				!(((fuf_flags&FUF_RECURSE) != 0) && (ent->d_type == DT_DIR)) && 
				filesfindmatch(full_name, files_find_mask, fuf_flags)) {
				
				struct CDirEntry *tempde = (struct CDirEntry *)malloc_for_templeos(sizeof(struct CDirEntry), data_heap, true);
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
	
	if (strcmp((char *)(t.Fs->cur_dir), "/") == 0) {
		if ((fuf_flags & FUF_JUST_DIRS) == 0){
			for (int i = 0; i < NUM_BUILTIN_FILES; ++i) {
				char *full_name = fileconcat(DRIVE_ROOT_PATH, builtin_files[i].name, true);
				if (!filesfindmatch(full_name, files_find_mask, fuf_flags)) {
					free_for_templeos(full_name);
					continue;
				}
				
				struct CDirEntry *tempde = (struct CDirEntry *)malloc_for_templeos(sizeof(struct CDirEntry), data_heap, true);
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

int64_t syscall_RedSeaFileWrite(uint64_t dv, char *cur_dir, char *name, char *buf, int64_t size, int64_t date, int64_t attr) {
	struct templeos_thread t;
	exit_templeos(&t);
	
	char *p = NULL;
	
	if (templeos_root != NULL) {
		char *p2 = fileconcat(templeos_root, cur_dir, false);
		struct stat statbuf;
		memset(&statbuf, 0, sizeof(struct stat));
		if (stat(p2, &statbuf) == 0) {
			p = fileconcat(p2, name, false);
		}
		free(p2);
	}
	
	if (p == NULL) {
		p = fileconcat(cur_dir, name, false);
	}
	
	int64_t clus = -1;
	
	int fd = open(p, O_CREAT|O_TRUNC|O_WRONLY, 0660);
	if (fd > 0) {
		int64_t rsz = size;
		while (rsz > 0) {
			int64_t n = write(fd, buf, rsz);
			if (n < 0) {
				if (errno == EAGAIN) {
					continue;
				}
				break;
			}
			rsz -= n;
			buf += n;
		}
		if (rsz == 0) {
			clus = intern_path(p);
		}
	}
	
	//TODO: do not ignore date and attr
		
	free(p);
	
	enter_templeos(&t);
	return clus;
}

bool syscall_RedSeaMkDir(uint64_t dv, char *cur_dir, char *name, int64_t entry_cnt) {
	struct templeos_thread t;
	exit_templeos(&t);
		
	char *p = NULL;
	
	if (templeos_root != NULL) {
		char *p2 = fileconcat(templeos_root, cur_dir, false);
		struct stat statbuf;
		memset(&statbuf, 0, sizeof(struct stat));
		if (stat(p2, &statbuf) == 0) {
			p = fileconcat(p2, name, false);
		}
		free(p2);
	}
	
	if (p == NULL) {
		p = fileconcat(cur_dir, name, false);
	}
	
	bool ok = false;
	
	if (mkdir(p, 0770) >= 0) {
		ok = true;
	}
	
	free(p);
	
	enter_templeos(&t);
	return ok;
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
	if (USE_GLIBC_MALLOC) {
		struct templeos_thread t;
		exit_templeos(&t);
		
		struct templeos_mem_entry_t *e = NULL;
		if (DEBUG_REGISTER_ALL_ALLOCATIONS || (((uint64_t)p) < 0x100000000)) {
			e = get_templeos_memory((uint64_t)p);
		}
		
		uint64_t r;
		if ((e != NULL) && e->is_mmapped) {
			r = (uint64_t)code_heap;
		} else {
			r = (uint64_t)data_heap;
		}
		
		// return heap ctrl for this location?
		
		enter_templeos(&t);
		return r;
	} else {
		return (uint64_t)stbm_get_heap(p);
	}
}

int64_t syscall_MSize(uint8_t *p) { // _MSIZE
	if (USE_GLIBC_MALLOC) {
		if (!DEBUG_REGISTER_ALL_ALLOCATIONS) {
		}
		
		struct templeos_thread t;
		exit_templeos(&t);
		
		struct templeos_mem_entry_t *e = get_templeos_memory((uint64_t)p);
		
		if (e == NULL) {
			// can't do this :'(
			uint64_t *p = NULL;
			*p = 0;
		}
		
		int64_t r = e->sz;
		
		enter_templeos(&t);
		return r;
	} else {
		return (int64_t)stbm_get_allocation_size(p);
	}
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
	
	struct timeval tv;
	memset(&tv, 0, sizeof(struct timeval));
	gettimeofday(&tv, NULL);
	
	struct tm tm;
	memset(&tm, 0, sizeof(struct tm));
	localtime_r(&tv.tv_sec, &tm);
	
	_ds->year = tm.tm_year + 1900;
	_ds->mon = tm.tm_mon + 1;
	_ds->day_of_mon = tm.tm_mday;
	_ds->day_of_week = tm.tm_wday;
	_ds->hour = tm.tm_hour;
	_ds->min = tm.tm_min;
	_ds->sec = tm.tm_sec;
	_ds->sec100 = tv.tv_usec / 10000;
	//TODO: sec10000
	
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

void syscall_Busy(int64_t s) {
	if (s == 0) {
		return;
	}
	struct templeos_thread t;
	exit_templeos(&t);
	
	usleep(s);
	
	enter_templeos(&t);
}

//void syscall_TaskDerivedValsUpdate(struct CTask *task, bool update_z_buf); NOP
//void syscall_Yield(void); // _YIELD; NOP
