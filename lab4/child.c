#include "unistd.h"
#include "stdio.h"
#include "stdlib.h"
#include "sys/mman.h"
#include "string.h"
#include "signal.h"
#include "fcntl.h"

#define check(VALUE, MSG, BADVAL) if (VALUE == BADVAL) { perror(MSG); exit(-1); }

char * add(char *str, int cap, int n, char c) {
	if (n == cap) {
		cap *= 2;
		str = (char *)realloc(str, sizeof(char) * cap);
		check(str, "realloc error", NULL)
	}
	str[n] = c;
	return str;
}

int main(int argc, char const *argv[]) {
	size_t pagesize = sysconf(_SC_PAGESIZE);
	char c = '\0';
	int n = 0;
	int cap = 256;
	char* str = (char *)malloc(sizeof(char) * cap);
	check(str, "malloc error", NULL)

	if (argc < 3) {
		perror("invalid child arguments");
		exit(-1);
	}
	int mfile = open(argv[1], O_RDWR);
	check(mfile, "open error", -1)
	int pid = atoi(argv[2]);

	char* fmap = (char *)mmap(NULL, pagesize, PROT_READ, MAP_SHARED, mfile, 0);
	check(fmap, "mmap error", MAP_FAILED)
	size_t i = 0;

	sigset_t set;
	check(sigemptyset(&set), "sigemptyset error", -1)
	check(sigaddset(&set, SIGUSR1), "sigaddset error", -1)
	check(sigprocmask(SIG_BLOCK, &set, NULL), "sigprocmask error", -1)
	int sig;

	for(;;) {
		check(sigwait(&set, &sig), "sigwait error", -1);
		for (i = 0; i < pagesize; ++i) {
			c = fmap[i];
			if (c != '\n' && c != '\0') {
				str = add(str, cap, n, c);
				++n;
			} else if (c == '\0') {
				break;
			} else {
				for (int i = n-1; i >= 0; i--) {
					printf("%c", str[i]);
				}
				printf("%c", c);
				n = 0;
				break;
			}
		}
		if (c == '\0') {
			break;
		} else {
			check(kill(pid, SIGUSR1), "send signal to parent error", -1)
		}
	}
	free(str);
	check(munmap(fmap, pagesize), "munmap error", -1)
	close(mfile);
}
