.PHONY: benchmark
benchmark:
	@gcc -Iruntime/inc runtime/libruntime.a benchmark.c -Wall -lm -D_GNU_SOURCE --std=gnu11 -o benchmark -Ofast -mavx -g -pthread
