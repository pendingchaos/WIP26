#include "runtime.h"

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
    for (size_t i = 0; i < 256; i++) system.property_dtypes[i] = PROP_FLOAT32;
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
    printf("VM Simulation:\n");
    printf("    %f nanoseconds per particle\n", (end-start)/1000000.0);
    printf("    %f particles per millisecond\n", 1.0/((end-start)/1000000000000.0));
    
    float* posx_arr = (float*)system.properties[get_property_index(&program, "position.x")];
    float* posy_arr = (float*)system.properties[get_property_index(&program, "position.y")];
    float* posz_arr = (float*)system.properties[get_property_index(&program, "position.z")];
    float* velx_arr = (float*)system.properties[get_property_index(&program, "velocity.x")];
    float* vely_arr = (float*)system.properties[get_property_index(&program, "velocity.y")];
    float* velz_arr = (float*)system.properties[get_property_index(&program, "velocity.z")];
    float* accx_arr = (float*)system.properties[get_property_index(&program, "accel.x")];
    float* accy_arr = (float*)system.properties[get_property_index(&program, "accel.y")];
    float* accz_arr = (float*)system.properties[get_property_index(&program, "accel.z")];
    float* colx_arr = (float*)system.properties[get_property_index(&program, "colorrgb.x")];
    float* coly_arr = (float*)system.properties[get_property_index(&program, "colorrgb.y")];
    float* colz_arr = (float*)system.properties[get_property_index(&program, "colorrgb.z")];
    start = get_time();
    for (size_t i = 0; i < 1000000; i++) {
        float posx = posx_arr[i];
        float posy = posy_arr[i];
        float posz = posz_arr[i];
        float velx = velx_arr[i];
        float vely = vely_arr[i];
        float velz = velz_arr[i];
        float accx = accx_arr[i];
        float accy = accy_arr[i];
        float accz = accz_arr[i];
        float colx = colx_arr[i];
        float coly = coly_arr[i];
        float colz = colz_arr[i];
        
        posx += velx;
        posy += vely;
        posz += velz;
        velx = velx*0.9 + accx;
        vely = vely*0.9 + accy;
        velz = velz*0.9 + accz;
        accx *= 0.8;
        accy *= 0.8;
        accz *= 0.8;
        
        float b = sqrt(velx*velx + vely*vely + velz*velz);
        b = fmin(fmax(b, 0.0f), 1.0f);
        
        colx = 0.1*(1.0f-b) + 1.0f*b;
        coly = colx;
        colz = colx;
        
        posx_arr[i] = posx;
        posy_arr[i] = posy;
        posz_arr[i] = posz;
        velx_arr[i] = velx;
        vely_arr[i] = vely;
        velz_arr[i] = velz;
        accx_arr[i] = accx;
        accy_arr[i] = accy;
        accz_arr[i] = accz;
        colx_arr[i] = colx;
        coly_arr[i] = coly;
        colz_arr[i] = colz;
    }
    end = get_time();
    printf("Native Simulation:\n");
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
