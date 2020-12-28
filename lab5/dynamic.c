#include "functions.h"

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#define check(VALUE, MSG, BADVAL) if (VALUE == BADVAL) { perror(MSG); exit(1); }

int main() {
	char* libnames[] = {"./libfunctions1.so", "./libfunctions2.so"};
	int lib = 0;
	int (*PrimeCount)(int, int) = NULL;
	float (*E)(int) = NULL;

	void *handle;
	handle = dlopen(libnames[lib], RTLD_NOW);
	check(handle, dlerror(), NULL)

	int f;
	int a, b, x;
	while (scanf("%d", &f) > 0) {
		if (f == 0) {
			if (dlclose(handle) != 0) {
				perror(dlerror());
				exit(1);
			}
			lib = (lib + 1) % 2;
			handle = dlopen(libnames[lib], RTLD_NOW);
			check(handle, dlerror(), NULL);

		} else if (f == 1) {
			if (scanf("%d %d", &a, &b) != 2) {
				perror("invalid input");
				exit(1);
			}
			PrimeCount = (int (*)(int, int))dlsym(handle, "PrimeCount");
			check(PrimeCount, dlerror(), NULL);
			printf("Prime numbers: %d\n", (*PrimeCount)(a, b));
		
		} else if (f == 2) {
			if (scanf("%d", &x) != 1) {
				perror("invalid input");
				exit(1);
			}
			E = (float (*)(int))dlsym(handle, "E");
			check(E, dlerror(), NULL);
			printf("e = %f\n", (*E)(x));
		}
	}
	if (dlclose(handle) != 0) {
		perror(dlerror());
		exit(1);
	}
}
