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
    if (!create_runtime(&runtime, "auto")) {
        fprintf(stderr, "Unable to create runtime: %s\n", runtime.error);
        return 1;
    }
    
    program_t program;
    program.runtime = &runtime;
    if (!open_program("benchmark.sim.bin", &program)) {
        fprintf(stderr, "Unable to open program.bin: %s\n", runtime.error);
        return 1;
    }
    
    system_t system;
    system.runtime = &runtime;
    system.pool_size = 1000000;
    system.sim_program = &program;
    for (size_t i = 0; i < 256; i++) system.attribute_dtypes[i] = ATTR_FLOAT32;
    create_system(&system);
    
    for (size_t i = 0; i < system.pool_size; i++)
        if (spawn_particle(&system) < 0)
            fprintf(stderr, "Unable to spawn particle: %s\n", runtime.error);
    
    uint64_t start_nano = get_time();
    uint64_t start_cycle = __rdtsc();
    for (size_t i = 0; i < 30; i++) {
        if (!simulate_system(&system)) {
            fprintf(stderr, "Unable to execute program: %s\n", runtime.error);
            destroy_program(&program);
            return 1;
        }
    }
    uint64_t end_cycle = __rdtsc();
    uint64_t end_nano = get_time();
    
    printf("%f nanoseconds per particle\n", (double)((end_nano-start_nano)/(__float128)(system.pool_size*30)));
    printf("%f cycles per particle\n", (double)((end_cycle-start_cycle)/(__float128)(system.pool_size*30)));
    
    if (!destroy_system(&system)) {
        fprintf(stderr, "Unable to destroy program: %s\n", runtime.error);
        return 1;
    }
    if (!destroy_program(&program)) {
        fprintf(stderr, "Unable to destroy program: %s\n", runtime.error);
        return 1;
    }
    if (!destroy_runtime(&runtime)) {
        fprintf(stderr, "Unable to destroy runtime: %s\n", runtime.error);
        return 1;
    }
    
    return 0;
}
