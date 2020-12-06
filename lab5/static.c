#include "functions.h"

#include <stdio.h>
#include <stdlib.h>

int main() {
	int f;
	int a, b, x;
	while (scanf("%d", &f) > 0) {
		if (f == 1) {
			if (scanf("%d %d", &a, &b) != 2) {
				perror("invalid input");
				exit(1);
			}
			printf("Prime numbers: %d\n", PrimeCount(a, b));
		
		} else if (f == 2) {
			if (scanf("%d", &x) != 1) {
				perror("invalid input");
				exit(1);
			}
			printf("e = %f\n", E(x));
		}
	}
}