.PHONY: tests
tests:
	@gcc -I../runtime/inc runtest.c -L/usr/lib64/llvm -lLLVM-3.7 ../runtime/libruntime.a -Wall -lm -D_DEFAULT_SOURCE --std=gnu11 -o runtest -Ofast -mavx -g -pthread
