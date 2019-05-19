uint64_t read_uint64(void *mem, off_t off) {
	return *((uint64_t *)(((char *)mem) + off));
}

void *malloc_executable_aligned(size_t size, int64_t alignment, int64_t misalignment) {
	int64_t mask=alignment-1;
	uint8_t *ptr = mmap(NULL, size+mask+sizeof(int64_t)+misalignment, PROT_EXEC|PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT, -1, 0);
	// note that MAP_32BIT is fundamental, this will break if the ptr3 can't be represented in 32bits, if linux ever stops honoring MAP_32BIT we're going to be fucked hard.
	uint8_t *res=(uint8_t *)((((uint64_t)(ptr+sizeof(int64_t)+mask))&~mask)+misalignment);
	//res(I64 *)[-1]=ptr-res; // wtf is this?
	register_templeos_memory(res, size, true);
	return res;
}

char *strclone(char *s) {
	char *r = malloc(strlen(s)+1);
	strcpy(r, s);
	return r;
}

bool extension_is(char *s, char *ext) {
	if (strlen(s) <= strlen(ext)) {
		return false;
	}
	
	char *p = s + strlen(s) - strlen(ext);
	return strcmp(p, ext) == 0;
}

void writestr(int fd, char *s) {
	char *p = s;
	int n = strlen(s);
	while (n > 0) {
		int r = write(fd, p, n);
		if (r == -1) {
			_exit(EXIT_FAILURE);
		}
		p += r;
		n -= r;
	}
}

#define STACKTRACE_PRINT_ARGS true

void signal_handler(int sig, siginfo_t *info, void *ucontext_void) {
	struct templeos_thread t;
	exit_templeos(&t);
	struct ucontext_t *ucontext = (struct ucontext_t *)ucontext_void;
	
	uint64_t rip = ucontext->uc_mcontext.gregs[REG_RIP];
	uint64_t rbp = ucontext->uc_mcontext.gregs[REG_RBP];
	uint64_t rsp = ucontext->uc_mcontext.gregs[REG_RSP];
	
	fprintf(stderr, "Received signal %d at 0x%lx\n", sig, rip);
	print_stack_trace(stderr, t.Fs, rip, rbp, rsp);
	
	fprintf(stderr, "\nRegisters:\n");
	
	fprintf(stderr, "\tRAX %016llx\n", ucontext->uc_mcontext.gregs[REG_RAX]);
	fprintf(stderr, "\tRBX %016llx\n", ucontext->uc_mcontext.gregs[REG_RBX]);
	fprintf(stderr, "\tRCX %016llx\n", ucontext->uc_mcontext.gregs[REG_RCX]);
	fprintf(stderr, "\tRDX %016llx\n", ucontext->uc_mcontext.gregs[REG_RDX]);
	fprintf(stderr, "\tRSI %016llx\n", ucontext->uc_mcontext.gregs[REG_RSI]);
	fprintf(stderr, "\tRDI %016llx\n", ucontext->uc_mcontext.gregs[REG_RDI]);
	fprintf(stderr, "\tR8  %016llx\n", ucontext->uc_mcontext.gregs[REG_R8]);
	fprintf(stderr, "\tR9  %016llx\n", ucontext->uc_mcontext.gregs[REG_R9]);
	fprintf(stderr, "\tR10 %016llx\n", ucontext->uc_mcontext.gregs[REG_R10]);
	fprintf(stderr, "\tR11 %016llx\n", ucontext->uc_mcontext.gregs[REG_R11]);
	fprintf(stderr, "\tR12 %016llx\n", ucontext->uc_mcontext.gregs[REG_R12]);
	fprintf(stderr, "\tR13 %016llx\n", ucontext->uc_mcontext.gregs[REG_R13]);
	fprintf(stderr, "\tR14 %016llx\n", ucontext->uc_mcontext.gregs[REG_R14]);
	fprintf(stderr, "\tR15 %016llx\n", ucontext->uc_mcontext.gregs[REG_R15]);
	
	if (DEBUG_PRINT_TEMPLEOS_SYMBOL_TABLE_ON_SIGNAL) {
		fprintf(stderr, "\nPrint system hash table:\n");
		print_templeos_hash_table(stderr, t.Fs);
	}
	
	fflush(stderr);
	_exit(EXIT_FAILURE);
}

struct hash_t paths_table;

uint64_t intern_path(char *p) {
	struct export_t *e = hash_get(&paths_table, p);
	if (e != NULL) {
		return e->val;
	}
	e = (struct export_t *)malloc(sizeof(struct export_t));
	char *k = strclone(p);
	e->type = 0;
	e->val = (uint64_t)(k);
	hash_put(&paths_table, k, e);
	return e->val;
}

char *fileconcat(char *p1, char *p2, bool for_templeos) {
	char *p;
	if (for_templeos) {
		p = malloc_for_templeos(strlen(p1) + strlen(p2) + 2, false, false);
	} else {
		p = malloc(strlen(p1) + strlen(p2) + 2);
	}
	strcpy(p, p1);
	if ((strlen(p) == 0) || (p[strlen(p)-1] != '/')) {
		strcat(p, "/");
	}
	strcat(p, p2);
	return p;
}

int Bt(uint8_t *bit_field, int bit_num) {
    bit_field+=bit_num>>3; // bit_field now points to the appropriate byte in src byte-array
    bit_num&=7; // Get the last 3 bits of bit_num ( 7 is 111 in binary). Basically bit_num % 8. Signifies which bit in the byte now specified by bit_num we are looking at.
    return (*bit_field & (1<<bit_num)) ? 1:0;
}

int Bts(uint8_t *bit_field, int bit_num) {
    int result;
    bit_field+=bit_num>>3; //Get the relevant byte (now in the dest bitfield)
    bit_num&=7; // Get the right bit in that byte

    result=*bit_field & (1<<bit_num); // Only used for return value

    *bit_field|=(1<<bit_num); //Make a byte with the relevant bit switched on and OR it on the relevant byte
    return (result) ? 1:0;
}
