// size of TOS binary header in bytes
#define TOSB_HEADER_SIZE 32

char *TOSB_SIGNATURE = "TOSB"; // signature at the start of all TempleOS binary files


void *load_file(char *path, size_t *psz) {
	struct stat statbuf;
	if (stat(path, &statbuf) != 0) {
		fflush(stdout);
		fprintf(stderr, "stat %s: %s\n", path, strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	void *r = malloc(statbuf.st_size);
	
	int fd = open(path, O_RDONLY);
	if (fd == -1) {
		fflush(stdout);
		fprintf(stderr, "opening %s: %s\n", path, strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	void *p = r;
	*psz = statbuf.st_size;
	int sz = statbuf.st_size;
	
	while (sz > 0) {
		int n = read(fd, p, sz);
		if (n < 0) {
			fflush(stdout);
			fprintf(stderr, "reading %s: %s\n", path, strerror(errno));
			exit(EXIT_FAILURE);
		}
		p += n;
		sz -= n;
	}
	
	if (close(fd) == -1) {
		fflush(stdout);
		fprintf(stderr, "closing %s: %s\n", path, strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	return r;
}
