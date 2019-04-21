uint64_t read_uint64(void *mem, off_t off) {
	return *((uint64_t *)(((char *)mem) + off));
}

void *malloc_executable_aligned(size_t size, int64_t alignment, int64_t misalignment) {
	int64_t mask=alignment-1;
	uint8_t *ptr = mmap(NULL, size+mask+sizeof(int64_t)+misalignment, PROT_EXEC|PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT, -1, 0);
	// note that MAP_32BIT is fundamental, this will break if the ptr3 can't be represented in 32bits, if linux ever stops honoring MAP_32BIT we're going to be fucked hard.
	uint8_t *res=(uint8_t *)((((uint64_t)(ptr+sizeof(int64_t)+mask))&~mask)+misalignment);
	//res(I64 *)[-1]=ptr-res; // wtf is this?
	return res;
}

char *strclone(char *s) {
	char *r = malloc(strlen(s)+1);
	strcpy(r, s);
	return r;
}
