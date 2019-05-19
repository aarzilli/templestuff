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
	
	void *p = malloc_for_templeos(size, mem_task == 0x02, false);
	
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

#define FUF_JUST_DIRS           0x0000400 //D
#define FUF_JUST_FILES          0x0000800 //F

#define RS_ATTR_DIR 0x10
#define RS_ATTR_COMPRESSED 0x400

int64_t syscall_RedSeaFileFind(uint64_t dv, int64_t cur_dir_clus, uint8_t *name, struct CDirEntry *_res, int64_t fuf_flags) {
	int64_t res = 0;
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
	
	if (((fuf_flags & FUF_JUST_DIRS) == 0) && (strcmp(dpath, "/") == 0)) {
		for (int i = 0; i < NUM_BUILTIN_FILES; ++i) {
			if (strcmp((char *)name, builtin_files[i].name) == 0) {
				if (_res != NULL) {
					_res->attr |= RS_ATTR_COMPRESSED;
				}
				strcpy((char *)(_res->name), builtin_files[i].name);
				_res->size = builtin_files[i].size;
				_res->datetime = 0;
				_res->clus = intern_path(fileconcat(dpath, builtin_files[i].name, false));
				if (DEBUG_FILE_SYSTEM) {
					printf("\tfound (%s)\n", builtin_files[i].name);
				}
				enter_templeos(&t);
				return 1;
			}
		}
	}
	
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
		char *p = fileconcat(dpath, ent->d_name, false);
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
		
		if (DEBUG_FILE_SYSTEM) {
			printf("\tfound %lx\n", _res->clus);
		}
		
		free(p);
	} else if (DEBUG_FILE_SYSTEM) {
		printf("\tnot found\n");
	}
	
	if (DEBUG_FILE_SYSTEM) {
		printf("cur dir: %s %p\n", t.Fs->cur_dir, t.Fs->cur_dir);
	}

	closedir(d);
	
	enter_templeos(&t);
	return res;
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
	
	char *p = fileconcat(cur_dir, filename, false);
	char *buf = NULL;
		
	struct stat statbuf;
	memset(&statbuf, 0, sizeof(struct stat));
	if (stat(p, &statbuf) == 0) {
		if (pattr != NULL) {
			*pattr = 0;
			if (extension_is(filename, ".Z")) {
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
		if (fd >= 0) {
			buf = (char *)malloc_for_templeos(statbuf.st_size+1, false, false);
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
					buf = NULL;
					break;
				}
				
				rdbuf += n;
				toread -= n;
			}
			
			buf[statbuf.st_size] = '\0';
			
			close(fd);
		} else {
			fprintf(stderr, "could not open %s: %s\n", p, strerror(errno));
		}
	}
	
	free(p);
	
	enter_templeos(&t);
	return buf;
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
