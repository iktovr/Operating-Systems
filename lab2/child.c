#include "unistd.h"
#include "stdio.h"
#include "stdlib.h"

char * add(char *str, int cap, int n, char c) {
	if (n == cap) {
		cap *= 2;
		str = (char *)realloc(str, sizeof(char) * cap);
		if (str == NULL) {
			perror("realloc error");
			exit(1);
		}
	}
	str[n] = c;
	return str;
}

int main() {
	char c;
	int n = 0;
	int cap = 256;
	char *str = (char *)malloc(sizeof(char) * cap);
	if (str == NULL) {
		perror("malloc error");
		return 1;
	}
	while(read(fileno(stdin), &c, sizeof(char)) > 0) {
		if (c != '\n') {
			str = add(str, cap, n, c);
			++n;
		} else {
			for (int i = n-1; i >= 0; i--) {
				printf("%c", str[i]);
			}
			printf("%c", c);
			n = 0;
		}
	}
	free(str);
}
