#include "runtime.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t get_time() {
    struct timespec t;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return t.tv_sec*(uint64_t)1000000000 + t.tv_nsec;
}

int main() {
    runtime_t runtime;
    if (!create_runtime(&runtime, "auto")) {
        fprintf(stderr, "Unable to create runtime: %s\n", runtime.error);
        return 1;
    }
    
    program_t program;
    program.runtime = &runtime;
    if (!open_program("compiler/test.sim.bin", &program)) {
        fprintf(stderr, "Unable to open program.bin: %s\n", runtime.error);
        return 1;
    }
    
    system_t system;
    system.runtime = &runtime;
    system.pool_size = 1000000;
    system.sim_program = &program;
    create_system(&system);
    
    uint64_t start = get_time();
    for (size_t i = 0; i < 1000000; i++)
        if (spawn_particle(&system) < 0)
            fprintf(stderr, "Unable to spawn particle: %s\n", runtime.error);
    uint64_t end = get_time();
    printf("Spawn:\n");
    printf("    %f nanoseconds per particle\n", (end-start)/1000000.0);
    printf("    %f particles per millisecond\n", 1.0f/((end-start)/1000000000000.0));
    
    start = get_time();
    if (!simulate_system(&system)) {
        fprintf(stderr, "Unable to execute program: %s\n", runtime.error);
        destroy_program(&program);
        return 1;
    }
    end = get_time();
    printf("Simulation:\n");
    printf("    %f nanoseconds per particle\n", (end-start)/1000000.0);
    printf("    %f particles per millisecond\n", 1.0/((end-start)/1000000000000.0));
    
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
