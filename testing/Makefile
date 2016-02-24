.PHONY: tests
tests:
	gcc -I../runtime/inc ../runtime/src/runtime.c ../runtime/src/vm_backend.c runtest.c -Wall -lm -D_DEFAULT_SOURCE --std=gnu11 -o runtest -Ofast -mavx
