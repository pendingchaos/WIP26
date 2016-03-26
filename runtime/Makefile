.PHONY: all
all:
	@gcc -Iinc -c src/runtime.c -Wall -lm -D_GNU_SOURCE --std=gnu11 -o runtime.o -Ofast -mavx -g -pthread
	@gcc -Iinc -c src/vm_backend.c -Wall -lm -D_GNU_SOURCE --std=gnu11 -o vm_backend.o -Ofast -mavx -g -pthread
	@gcc -Iinc -c src/threading.c -Wall -lm -D_GNU_SOURCE --std=gnu11 -o threading.o -Ofast -mavx -g -pthread
	@ar rcs libruntime.a runtime.o vm_backend.o threading.o
	@rm runtime.o
	@rm vm_backend.o
	@rm threading.o
