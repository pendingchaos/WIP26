.PHONY: vm
vm:
	gcc -Iruntime/inc runtime/src/runtime.c runtime/src/vm_backend.c main.c -Wall -lm -D_DEFAULT_SOURCE --std=gnu11 -o main -Ofast -mavx
