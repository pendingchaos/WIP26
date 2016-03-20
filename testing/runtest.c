#include "runtime.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_DIFF 0.0001f
#define MAX_ULP_DIFF 100

static int32_t get_floati(float f) {
    union {
        float f;
        int32_t i;
    } u;
    u.f = f;
    return u.i;
}

//https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
static bool float_equal(float a, float b) {
    if (fabs(a-b) <= MAX_DIFF) return true;
    
    int32_t ai = get_floati(a);
    int32_t bi = get_floati(b);
    
    if ((ai<0) != (bi<0)) return false;
    
    if (abs(ai-bi) <= MAX_ULP_DIFF) return true;
    
    return false;
}

int main(int argc, char** argv) {
    runtime_t runtime;
    if (!create_runtime(&runtime, "vm")) {
        fprintf(stderr, "Failed to create runtime: %s\n", runtime.error);
        return 1;
    }
    
    char prog[1024];
    snprintf(prog, sizeof(prog), "%s.bin", argv[1]);
    
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "../compiler/compiler -I../compiler/ -i %s -o %s -t sim", argv[1], prog);
    system(cmd);
    
    int count = atoi(argv[2]);
    
    program_t program;
    program.runtime = &runtime;
    if (!open_program(prog, &program)) {
        fprintf(stderr, "Failed to open %s: %s\n", prog, runtime.error);
        return 1;
    }
    
    particles_t particles;
    particles.runtime = &runtime;
    if (!create_particles(&particles, count)) {
        fprintf(stderr, "Failed to create particles: %s\n", runtime.error);
        return 1;
    }
    
    for (int i = 3; i<argc;) {
        if (argv[i][0] == 'p') {
            i++;
            const char* name = argv[i];
            const char* input = argv[i+1];
            int particle_index = atoi(argv[i+3]);
            
            int index = -1;
            for (size_t i = 0; i < 256; i++)
                if (particles.attribute_names[i] && !strcmp(particles.attribute_names[i], name)) {
                    index = i;
                    break;
                }
            if (index < 0)
                if (!add_attribute(&particles, name, ATTR_FLOAT32, &index)) {
                    fprintf(stderr, "Failed to add attribute \"%s\"\n", name);
                    return 1;
                }
            
            ((float*)particles.attributes[index])[particle_index] = atof(input);
            i += 4;
        } else if (argv[i][0] == 'u') i += 3;
    }
    
    for (size_t i = 0; i < count; i++)
        if (spawn_particle(&particles) < 0) {
            fprintf(stderr, "Failed to spawn particle: %s\n", runtime.error);
            return 1;
        }
    
    system_t system;
    system.runtime = &runtime;
    system.particles = &particles;
    system.sim_program = &program;
    system.emit_program = NULL;
    if (!create_system(&system)) {
        fprintf(stderr, "Failed to create particle system: %s\n", runtime.error);
        return 1;
    }
    for (int i = 3; i<argc;) {
        if (argv[i][0] == 'p') i += 5;
        else if (argv[i][0] == 'u') {
            i++;
            const char* name = argv[i];
            const char* input = argv[i+1];
            
            int index = get_uniform_index(&program, name);
            if (index < 0) {
                fprintf(stderr, "Failed to find uniform \"%s\"\n", name);
                return 1;
            }
            
            system.sim_uniforms[index] = atof(input);
            i += 2;
        }
    }
    
    if (!simulate_system(&system)) {
        fprintf(stderr, "Failed to execute program: %s\n", runtime.error);
        destroy_program(&program);
        return 1;
    }
    
    for (int i = 3; i<argc;)
        if (argv[i][0] == 'p') {
            i++;
            const char* name = argv[i];
            const char* expected = argv[i+2];
            int particle_index = atoi(argv[i+3]);
            
            int index = 0;
            for (size_t i = 0; i < 256; i++)
                if (particles.attribute_names[i] && !strcmp(particles.attribute_names[i], name)) {
                    index = i;
                    break;
                }
            
            float val = ((float*)particles.attributes[index])[particle_index];
            if (!float_equal(val, atof(expected))) {
                fprintf(stderr, "Incorrect value for attribute \"%s\" for particle %d. Expected %f. Got %f\n",
                        name, particle_index, atof(expected), val);
                return 1;
            }
            i += 4;
        } else if (argv[i][0] == 'u') i += 3;
    
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
