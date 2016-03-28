.PHONY: build
build: main.sim.bin main.emit.bin
	@gcc -I../runtime/inc main.c -L/usr/lib64/llvm -lLLVM-3.7 ../runtime/libruntime.a -Wall -lm -D_DEFAULT_SOURCE --std=gnu11 -o main -Ofast -mavx -g `pkg-config glew --cflags --libs` `pkg-config glfw3 --cflags --libs` -pthread

main.sim.bin: main.sim
	@../compiler/compiler -i main.sim -o main.sim.bin -t sim -I ../compiler/

main.emit.bin: main.emit
	@../compiler/compiler -i main.emit -o main.emit.bin -t emit -I ../compiler/
