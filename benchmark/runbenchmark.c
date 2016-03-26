#include "runtime.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

static uint64_t get_time() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec*(uint64_t)1000000000 + t.tv_nsec;
}

int main(int argc, char** argv) {
    char prog[1024];
    snprintf(prog, sizeof(prog), "%s.bin", argv[1]);
    
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "python assembler.py %s %s", argv[1], prog);
    system(cmd);
    
    runtime_t runtime;
    if (!create_runtime(&runtime, NULL)) {
        fprintf(stderr, "Failed to create runtime: %s\n", runtime.error);
        return 1;
    }
    
    program_t program;
    program.runtime = &runtime;
    if (!open_program(prog, &program)) {
        fprintf(stderr, "Failed to open %s: %s\n", prog, runtime.error);
        return 1;
    }
    
    particles_t particles;
    particles.runtime = &runtime;
    if (!create_particles(&particles, 2000000)) {
        fprintf(stderr, "Failed to create particles: %s\n", runtime.error);
        return 1;
    }
    
    system_t system;
    system.runtime = &runtime;
    system.particles = &particles;
    system.sim_program = &program;
    system.emit_program = NULL;
    if (!create_system(&system)) {
        fprintf(stderr, "Failed to create particle system: %s\n", runtime.error);
    }
    
    for (size_t i = 0; i < particles.pool_size; i++)
        if (spawn_particle(&particles) < 0)
            fprintf(stderr, "Failed to spawn particle: %s\n", runtime.error);
    
    uint64_t start_nano = get_time();
    for (size_t i = 0; i < 10; i++) {
        if (!simulate_system(&system)) {
            fprintf(stderr, "Failed to execute program: %s\n", runtime.error);
            destroy_program(&program);
            return 1;
        }
    }
    uint64_t end_nano = get_time();
    
    printf("%f nanoseconds per particle\n", (double)((end_nano-start_nano)/(__float128)(particles.pool_size*10)));
    
    if (!destroy_system(&system)) {
        fprintf(stderr, "Failed to destroy program: %s\n", runtime.error);
        return 1;
    }
    if (!destroy_particles(&particles)) {
        fprintf(stderr, "Failed to destroy particles: %s\n", runtime.error);
        return 1;
    }
    if (!destroy_program(&program)) {
        fprintf(stderr, "Failed to destroy program: %s\n", runtime.error);
        return 1;
    }
    if (!destroy_runtime(&runtime)) {
        fprintf(stderr, "Failed to destroy runtime: %s\n", runtime.error);
        return 1;
    }
    
    return 0;
}
