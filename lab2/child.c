#include "unistd.h"
#include "stdio.h"

int main() {
	char c;
	char str[256];
	int n = 0;
	while(read(fileno(stdin), &c, sizeof(char)) > 0) {
		if (c != '\n') {
			str[n] = c;
			++n;
		} else {
			for (int i = n-1; i >= 0; i--) {
				printf("%c", str[i]);
			}
			printf("%c", c);
			n = 0;
		}
	}
}