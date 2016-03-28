.PHONY: all
all:
	@gcc -I../runtime/inc runbenchmark.c -L/usr/lib64/llvm -lLLVM-3.7 ../runtime/libruntime.a -Wall -lm -D_GNU_SOURCE --std=gnu11 -o runbenchmark -Ofast -mavx -g -pthread
