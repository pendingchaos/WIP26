.PHONY: build
build: main.sim.bin
	gcc -I../runtime/inc ../runtime/src/runtime.c ../runtime/src/vm_backend.c main.c -Wall -lm -D_DEFAULT_SOURCE --std=gnu11 -o main -Ofast -mavx -g `pkg-config glew --cflags --libs` `pkg-config glfw3 --cflags --libs`

main.sim.bin: main.sim
	../compiler/compiler -i main.sim -o main.sim.bin -t sim -I ../compiler/
