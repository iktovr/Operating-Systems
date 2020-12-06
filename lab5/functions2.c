#include "functions.h"

#include <stdlib.h>
#include <stdio.h>

int factorial(int x) {
	int result = 1;
	for (int i = 2; i <= x; ++i) {
		result *= i;
	}
	return result;
}

float E(int x) {
	float result = 0;
	for (int n = 0; n <= x; ++n) {
		result += 1.0 / factorial(n);
	}
	return result;
}

int PrimeCount(int A, int B) {
	int n = 0;
	int *sieve = (int *)malloc(sizeof(int) * (B+1));
	if (sieve == NULL) {
		perror("malloc error");
		exit(1);
	}
	for (int i = 0; i <= B; ++i) sieve[i] = 0;
	for (int i = 2; i <= B; ++i) {
		if (sieve[i] != 0) {
			continue;
		}
		++n;
		for (int j = i + i; j <= B; j += i) {
			sieve[j] = 1;
		}
	}
	return n;
}
