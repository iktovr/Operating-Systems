#include "unistd.h"
#include "stdio.h"

int main() {
	char fn1[256];
	char fn2[256];
	scanf("%s", fn1);
	scanf("%s", fn2);
	FILE* file1 = fopen(fn1, "wt");
	if (file1 == NULL) {
		perror("fopen error");
		return 1;
	}
	FILE* file2 = fopen(fn2, "wt");
	if (file2 == NULL) {
		perror("fopen error");
		return 1;
	}

	int fd1[2], fd2[2];
	if (pipe(fd1) != 0) {
		perror("pipe error");
		return 1;
	}
	if (pipe(fd2) != 0) {
		perror("pipe error");
		return 1;
	}

	int id1 = fork();

	if (id1 == -1)	{
		perror("fork error");
		return -1;
		
	} else if (id1 == 0) {
		fclose(file2);
		close(fd2[0]);
		close(fd2[1]);
		close(fd1[1]);
		if (dup2(fd1[0], fileno(stdin)) == -1) {
			perror("dup2 error");
			return 1;
		}
		if (dup2(fileno(file1), fileno(stdout)) == -1) {
			perror("dup2 error");
			return 1;
		}
		fclose(file1);

		char * const * null = NULL;
		if (execv("child", null) == -1) {
			perror("execv error");
			return 1;
		}

	} else {
		int id2 = fork();

		if (id2 == -1)	{
			perror("fork error");
			return -1;

		} else if (id2 == 0) {
			fclose(file1);
			close(fd1[0]);
			close(fd1[1]);
			close(fd2[1]);
			if (dup2(fd2[0], fileno(stdin)) == -1) {
				perror("dup2 error");
				return 1;
			}
			if (dup2(fileno(file2), fileno(stdout)) == -1) {
				perror("dup2 error");
				return 1;
			}
			fclose(file2);

			char * const * null = NULL;
			if (execv("child", null) == -1) {
				perror("execv error");
				return 1;
			}

		} else {
			close(fd1[0]);
			close(fd2[0]);
			fclose(file1);
			fclose(file2);

			char c;
			char str[10];
			str[0] = '\0';
			int n = 0;
			while (scanf("%c", &c) > 0) {
				if (c != '\n') {
					if (n < 10) {
						str[n] = c;
					} else if (str[0] != '\0') {
						for (int i = 0; i < 10; ++i) {
							write(fd2[1], &str[i], sizeof(char));
							str[i] = '\0';
						}
						write(fd2[1], &c, sizeof(char));
					} else {
						write(fd2[1], &c, sizeof(char));
					}
					++n;
				} else {
					if (str[0] != '\0') {
						for (int i = 0; i < n; ++i) {
							write(fd1[1], &str[i], sizeof(char));
							str[i] = '\0';
						}
						write(fd1[1], &c, sizeof(char));
					} else {
						write(fd2[1], &c, sizeof(char));
					}
					n = 0;
				}
			}

			close(fd1[1]);
			close(fd2[1]);
		}
	}
	return 0;
}