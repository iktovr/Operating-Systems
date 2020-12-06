all: static dynamic libfunctions

static: libfunctions
	gcc -o static static.c -L./ -lfunctions2 -Wl,-rpath=./

dynamic: libfunctions
	gcc -rdynamic -o dynamic dynamic.c -ldl

libfunctions: functions
	gcc -shared -o libfunctions1.so functions1.o -lm
	gcc -shared -o libfunctions2.so functions2.o

functions: functions.h functions1.c functions2.c
	gcc -fPIC -c functions1.c
	gcc -fPIC -c functions2.c

clean:
	rm -f *.so *.o static dynamic