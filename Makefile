benchmark: benchmark.c runtime/src/runtime.c runtime/src/vm_backend.c runtime/inc/runtime.h
	gcc -Iruntime/inc runtime/src/runtime.c runtime/src/vm_backend.c runtime/src/threading.c benchmark.c -Wall -lm -D_GNU_SOURCE --std=gnu11 -o benchmark -Ofast -mavx -g
