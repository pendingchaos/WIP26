#include "runtime.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <endian.h>
#include <math.h>
#include <stdio.h>

bool vm_backend(backend_t* backend);

bool create_runtime(runtime_t* runtime, const char* backend) {
    backend_t vm;
    bool vm_supported = vm_backend(&vm);
    
    if (strcmp(backend, "auto")==0) backend = "vm";
    
    if (strcmp(backend, "vm")==0 && vm_supported) runtime->backend = vm;
    else return false;
    
    memset(runtime->error, 0, sizeof(runtime->error));
    
    return runtime->backend.create(runtime);
}

bool destroy_runtime(runtime_t* runtime) {
    return runtime->backend.destroy(runtime);
}

bool set_error(runtime_t* runtime, const char* message) {
    strncpy(runtime->error, message, sizeof(runtime->error));
    return false;
}

bool open_program(const char* filename, program_t* program) {
    FILE* f = fopen(filename, "rb");
    if (!f) return set_error(program->runtime, "Failed to open file");
    
    program->bc = NULL;
    
    char magic[8];
    if (!fread(magic, 8, 1, f)) return set_error(program->runtime, "Failed to read magic");
    if (memcmp(magic, "SIMv0.0 ", 8)==0) program->type = PROGRAM_TYPE_SIMULATION;
    else if (memcmp(magic, "EMTv0.0 ", 8)==0) program->type = PROGRAM_TYPE_EMITTER;
    else return set_error(program->runtime, "Invalid magic");
    
    if (!fread(&program->attribute_count, 1, 1, f)) return set_error(program->runtime, "Failed to read attribute count");
    if (!fread(&program->uniform_count, 1, 1, f)) return set_error(program->runtime, "Failed to read uniform count");
    if (!fread(&program->bc_size, 4, 1, f)) return set_error(program->runtime, "Failed to read bytecode size");
    program->bc_size = le32toh(program->bc_size);
    
    for (uint8_t i = 0; i < program->attribute_count; i++) {
        uint8_t len;
        if (!fread(&len, 1, 1, f)) {
            destroy_program(program);
            return set_error(program->runtime, "Failed to read attribute name length");
        }
        
        program->attribute_names[i] = calloc(len+1, 1);
        if (!program->attribute_names[i]) {
            destroy_program(program);
            return set_error(program->runtime, "Failed to allocate attribute name");
        }
        
        if (!fread(program->attribute_names[i], len, 1, f)) {
            destroy_program(program);
            return set_error(program->runtime, "Failed to read attribute name");
        }
        
        if (program->type == PROGRAM_TYPE_SIMULATION) {
            if (!fread(&program->attribute_load_regs[i], 1, 1, f)) {
                destroy_program(program);
                return set_error(program->runtime, "Failed to read attribute load register");
            }
            
            if (!fread(&program->attribute_store_regs[i], 1, 1, f)) {
                destroy_program(program);
                return set_error(program->runtime, "Failed to read attribute store register");
            }
        }
    }
    
    for (uint8_t i = 0; i < program->uniform_count; i++) {
        uint8_t len;
        if (!fread(&len, 1, 1, f)) {
            destroy_program(program);
            return set_error(program->runtime, "Failed to read uniform name length");
        }
        
        program->uniform_names[i] = calloc(len+1, 1);
        if (!program->uniform_names[i]) {
            destroy_program(program);
            return set_error(program->runtime, "Failed to allocate uniform name");
        }
        
        if (!fread(program->uniform_names[i], len, 1, f)) {
            destroy_program(program);
            return set_error(program->runtime, "Failed to read uniform name");
        }
        
        if (!fread(&program->uniform_regs[i], 1, 1, f)) {
            destroy_program(program);
            return set_error(program->runtime, "Failed to read uniform register");
        }
    }
    
    program->bc = malloc(program->bc_size+1);
    if (!program->bc) {
        destroy_program(program);
        return set_error(program->runtime, "Failed to allocate bytecode");
    }
    if (!fread(program->bc, program->bc_size, 1, f) && program->bc_size) {
        destroy_program(program);
        return set_error(program->runtime, "Failed to read bytecode");
    }
    program->bc[program->bc_size++] = BC_OP_END;
    
    fclose(f);
    
    if (!validate_program(program)) return false;
    
    return program->runtime->backend.create_program(program);
}

bool destroy_program(program_t* program) {
    bool success = true;
    if (!program->runtime->backend.destroy_program(program))
        success = false;
    for (uint8_t i = 0; i < program->attribute_count; i++)
        free(program->attribute_names[i]);
    free(program->bc);
    return success;
}

bool validate_program(const program_t* program) { //TODO: Validate conditional bytecode
    const uint8_t* bc = program->bc;
    const uint8_t* end = bc + program->bc_size;
    bool has_end_inst = false;
    while (bc != end) {
        bc_op_t op = *bc++;
        size_t required = 0;
        switch (op) {
        case BC_OP_RAND: required = 1; break;
        case BC_OP_MOVF: required = 5; break;
        case BC_OP_ADD:
        case BC_OP_SUB:
        case BC_OP_MUL:
        case BC_OP_DIV:
        case BC_OP_POW:
        case BC_OP_LESS:
        case BC_OP_GREATER:
        case BC_OP_EQUAL:
        case BC_OP_BOOL_AND:
        case BC_OP_BOOL_OR: required = 3; break;
        case BC_OP_SQRT:
        case BC_OP_BOOL_NOT:
        case BC_OP_FLOOR: required = 2; break;
        case BC_OP_SEL: required = 4; break;
        case BC_OP_COND_BEGIN: required = 7; break;
        case BC_OP_WHILE_BEGIN: required = 13; break;
        case BC_OP_COND_END:
        case BC_OP_WHILE_END:
        case BC_OP_WHILE_END_COND:
        case BC_OP_DELETE: required = 0; break;
        case BC_OP_END: required = 0; has_end_inst = true; break;
        case BC_OP_EMIT:
            if (!(end-bc))
                return set_error(program->runtime, "Unexpected end of bytecode");
            required = *bc++;
            break;
        default: return set_error(program->runtime, "Unsupported or invalid instruction.");
        }
        if (end-bc < required)
            return set_error(program->runtime, "Unexpected end of bytecode");
        bc += required;
    }
    
    if (!has_end_inst) return set_error(program->runtime, "No end instruction");
    else return true;
}

int get_attribute_index(const program_t* program, const char* name) {
    for (uint8_t i = 0; i < program->attribute_count; i++)
        if (strcmp(program->attribute_names[i], name) == 0) return i;
    return -1;
}

int get_uniform_index(const program_t* program, const char* name) {
    for (uint8_t i = 0; i < program->uniform_count; i++)
        if (strcmp(program->uniform_names[i], name) == 0) return i;
    return -1;
}

size_t get_attribute_padding(const runtime_t* runtime) {
    return runtime->backend.get_attribute_padding(runtime);
}

static size_t get_attr_dtype_size(attr_dtype_t dtype) {
    switch (dtype) {
    case ATTR_UINT8:
    case ATTR_INT8: return 1;
    case ATTR_UINT16:
    case ATTR_INT16: return 2;
    case ATTR_UINT32:
    case ATTR_INT32:
    case ATTR_FLOAT32: return 4;
    case ATTR_FLOAT64: return 8;
    }
    return 0;
}

bool create_particles(particles_t* particles, size_t pool_size) {
    size_t padding = get_attribute_padding(particles->runtime);
    pool_size = ceil(pool_size/(double)padding) * padding;
    
    particles->pool_size = pool_size;
    particles->pool_usage = 0;
    particles->nexts = malloc(pool_size*sizeof(int));
    if (!particles->nexts && pool_size) return set_error(particles->runtime, "Failed to allocate next indices");
    if (pool_size) {
        for (int i = 0; i < particles->pool_size-1; i++)
            particles->nexts[i] = i + 1;
        particles->nexts[pool_size-1] = -1;
    }
    particles->next_particle = 0;
    
    memset(particles->attribute_names, 0, sizeof(particles->attribute_names));
    memset(particles->attributes, 0, sizeof(particles->attributes));
    
    particles->deleted_flags = malloc(pool_size);
    if (!particles->deleted_flags && pool_size) {
        free(particles->nexts);
        particles->nexts = NULL;
        return set_error(particles->runtime, "Failed to allocate deleted flags");
    }
    memset(particles->deleted_flags, 1, pool_size);
    
    return true;
}

bool destroy_particles(particles_t* particles) {
    for (size_t i = 0; i < 256; i++) free(particles->attributes[i]);
    for (size_t i = 0; i < 256; i++) free(particles->attribute_names[i]);
    free(particles->nexts);
    free(particles->deleted_flags);
    return true;
}

bool add_attribute(particles_t* particles, const char* name, attr_dtype_t dtype, int* index) {
    size_t i = 0;
    for (; i < 256; i++)
        if (particles->attribute_names[i] && !strcmp(particles->attribute_names[i], name)) {
            free(particles->attribute_names);
            free(particles->attributes[i]);
        } else if (!particles->attribute_names[i]) break;
    
    particles->attribute_dtypes[i] = dtype;
    
    particles->attribute_names[i] = calloc(1, strlen(name)+1);
    if (!particles->attribute_names[i])
        return set_error(particles->runtime, "Failed to allocate attribute name");
    strcpy(particles->attribute_names[i], name);
    
    size_t size = get_attr_dtype_size(dtype);
    particles->attributes[i] = calloc(1, particles->pool_size*size);
    if (!particles->attributes[i])
        return set_error(particles->runtime, "Failed to allocate attribute data");
    
    if (index) *index = i;
    
    return true;
}

bool create_system(system_t* system) {
    if (system->sim_program && system->sim_program->type != PROGRAM_TYPE_SIMULATION)
        return set_error(system->runtime, "Simulation program is not a simulation program");
    if (system->emit_program && system->emit_program->type != PROGRAM_TYPE_EMITTER)
        return set_error(system->runtime, "Emitter program is not a emitter program");
    
    memset(system->sim_uniforms, 0, sizeof(system->sim_uniforms));
    memset(system->emit_uniforms, 0, sizeof(system->emit_uniforms));
    
    particles_t* particles = system->particles;
    
    if (system->sim_program) {
        for (size_t i = 0; i < system->sim_program->attribute_count; i++) {
            char* name = system->sim_program->attribute_names[i];
            int index = -1;
            for (size_t j = 0; j < 256; j++)
                if (particles->attribute_names[j] && !strcmp(particles->attribute_names[j], name)) {
                    index = j;
                    break;
                }
            if (index < 0)
                return set_error(system->runtime, "Unable to find attribute index in particles_t");
            system->sim_attribute_indices[i] = index;
        }
    }
    
    if (system->emit_program) {
        for (size_t i = 0; i < system->emit_program->attribute_count; i++) {
            char* name = system->emit_program->attribute_names[i];
            int index = -1;
            for (size_t j = 0; j < 256; j++)
                if (particles->attribute_names[j] && !strcmp(particles->attribute_names[j], name)) {
                    index = j;
                    break;
                }
            if (index < 0)
                return set_error(system->runtime, "Unable to find attribute index in particles_t");
            system->emit_attribute_indices[i] = index;
        }
    }
    
    return true;
}

bool destroy_system(system_t* system) {
    return true;
}

bool simulate_system(system_t* system) {
    return system->runtime->backend.simulate_system(system);
}

int spawn_particle(particles_t* particles) {
    int index = particles->next_particle;
    if (index < 0) {
        set_error(particles->runtime, "Pool is full");
        return -1;
    }
    
    particles->next_particle = particles->nexts[index];
    particles->deleted_flags[index] = 0;
    particles->pool_usage++;
    
    return index;
}

bool delete_particle(particles_t* particles, int index) {
    if (index < 0 || index >= particles->pool_size)
        return set_error(particles->runtime, "Invalid particle index");
    
    particles->deleted_flags[index] = 1;
    particles->nexts[index] = particles->next_particle;
    particles->next_particle = index;
    particles->pool_usage--;
    
    return true;
}
