CFLAGS = `llvm-config --cflags` -Ofast -mavx -g -pthread --std=gnu11 -D_GNU_SOURCE -Wall

.PHONY: all
all:
	@gcc -Iinc -c src/runtime.c -o runtime.o $(CFLAGS)
	@gcc -Iinc -c src/vm_backend.c -o vm_backend.o $(CFLAGS)
	@gcc -Iinc -c src/llvm_backend.c -o llvm_backend.o $(CFLAGS)
	@gcc -Iinc -c src/threading.c -o threading.o $(CFLAGS)
	@ar rcs libruntime.a runtime.o llvm_backend.o vm_backend.o threading.o
	@rm runtime.o
	@rm vm_backend.o
	@rm llvm_backend.o
	@rm threading.o
