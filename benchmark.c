#include "runtime.h"

#include <sched.h>
#include <unistd.h>
#include <x86intrin.h>
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t get_time() {
    struct timespec t;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return t.tv_sec*(uint64_t)1000000000 + t.tv_nsec;
}

int main() {
    cpu_set_t cpus;
    CPU_ZERO(&cpus);
    CPU_SET(0, &cpus);
    sched_setaffinity(getpid(), sizeof(cpu_set_t), &cpus);
    
    system("compiler/compiler -Icompiler/ -i benchmark.sim -o benchmark.sim.bin -t sim");
    
    runtime_t runtime;
    if (!create_runtime(&runtime, NULL)) {
        fprintf(stderr, "Failed to create runtime: %s\n", runtime.error);
        return 1;
    }
    
    program_t program;
    program.runtime = &runtime;
    if (!open_program("benchmark.sim.bin", &program)) {
        fprintf(stderr, "Failed to open program.bin: %s\n", runtime.error);
        return 1;
    }
    
    particles_t particles;
    particles.runtime = &runtime;
    if (!create_particles(&particles, 1000000)) {
        fprintf(stderr, "Failed to create particles: %s\n", runtime.error);
        return 1;
    }
    add_attribute(&particles, "position.x", ATTR_FLOAT32, NULL);
    add_attribute(&particles, "position.y", ATTR_FLOAT32, NULL);
    add_attribute(&particles, "position.z", ATTR_FLOAT32, NULL);
    add_attribute(&particles, "velocity.x", ATTR_FLOAT32, NULL);
    add_attribute(&particles, "velocity.y", ATTR_FLOAT32, NULL);
    add_attribute(&particles, "velocity.z", ATTR_FLOAT32, NULL);
    
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
    uint64_t start_cycle = __rdtsc();
    for (size_t i = 0; i < 30; i++) {
        if (!simulate_system(&system)) {
            fprintf(stderr, "Failed to execute program: %s\n", runtime.error);
            destroy_program(&program);
            return 1;
        }
    }
    uint64_t end_cycle = __rdtsc();
    uint64_t end_nano = get_time();
    
    printf("%f nanoseconds per particle\n", (double)((end_nano-start_nano)/(__float128)(particles.pool_size*30)));
    printf("%f cycles per particle\n", (double)((end_cycle-start_cycle)/(__float128)(particles.pool_size*30)));
    
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
