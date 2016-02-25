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
    if (!f) return set_error(program->runtime, "Unable to open file");
    
    program->bc = NULL;
    
    char magic[8];
    if (!fread(magic, 8, 1, f)) return set_error(program->runtime, "Unable to read magic");
    if (memcmp(magic, "SIMv0.0 ", 8)==0) program->type = PROGRAM_TYPE_SIMULATION;
    else if (memcmp(magic, "EMTv0.0 ", 8)==0) program->type = PROGRAM_TYPE_EMITTER;
    else return set_error(program->runtime, "Invalid magic");
    
    if (!fread(&program->property_count, 1, 1, f)) return set_error(program->runtime, "Unable to read property count");
    if (!fread(&program->bc_size, 4, 1, f)) return set_error(program->runtime, "Unable to read bytecode size");
    program->bc_size = le32toh(program->bc_size);
    
    for (uint8_t i = 0; i < program->property_count; i++) {
        uint8_t len;
        if (!fread(&len, 1, 1, f)) {
            destroy_program(program);
            return set_error(program->runtime, "Unable to read property name length");
        }
        
        program->property_names[i] = calloc(len+1, 1);
        if (!program->property_names[i]) {
            destroy_program(program);
            return set_error(program->runtime, "Unable to allocate property name");
        }
        
        if (!fread(program->property_names[i], len, 1, f)) {
            destroy_program(program);
            return set_error(program->runtime, "Unable to read property name");
        }
        
        if (!fread(&program->property_indices[i], 1, 1, f)) {
            destroy_program(program);
            return set_error(program->runtime, "Unable to read property index");
        }
    }
    
    program->bc = malloc(program->bc_size+1);
    if (!program->bc) {
        destroy_program(program);
        return set_error(program->runtime, "Unable to allocate bytecode");
    }
    if (!fread(program->bc, program->bc_size, 1, f) && program->bc_size) {
        destroy_program(program);
        return set_error(program->runtime, "Unable to read bytecode");
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
    for (uint8_t i = 0; i < program->property_count; i++)
        free(program->property_names[i]);
    free(program->bc);
    return success;
}

bool validate_program(const program_t* program) { //TODO: Validate property indices, register indices, conditional bytecode
    const uint8_t* bc = program->bc;
    const uint8_t* end = bc + program->bc_size;
    bool has_end_inst = false;
    while (bc != end) {
        bc_op_t op = *bc++;
        size_t required = 0;
        switch (op) {
        case BC_OP_ADD_R_F:
        case BC_OP_SUB_R_F:
        case BC_OP_SUB_F_R:
        case BC_OP_MUL_R_F:
        case BC_OP_DIV_R_F:
        case BC_OP_DIV_F_R:
        case BC_OP_POW_R_F:
        case BC_OP_POW_F_R:
        case BC_OP_MIN_R_F:
        case BC_OP_MAX_R_F: required = 6; break;
        case BC_OP_MOV_F: required = 5; break;
        case BC_OP_ADD_RR:
        case BC_OP_SUB_RR:
        case BC_OP_MUL_RR:
        case BC_OP_DIV_RR:
        case BC_OP_POW_RR:
        case BC_OP_MIN_RR:
        case BC_OP_LESS:
        case BC_OP_GREATER:
        case BC_OP_EQUAL:
        case BC_OP_BOOL_AND:
        case BC_OP_BOOL_OR:
        case BC_OP_MAX_RR: required = 3; break;
        case BC_OP_SQRT:
        case BC_OP_BOOL_NOT:
        case BC_OP_LOAD_PROP:
        case BC_OP_STORE_PROP: required = 2; break;
        case BC_OP_COND_BEGIN: required = 7; break;
        case BC_OP_COND_END:
        case BC_OP_END: required = 0; has_end_inst = true; break;
        default: return set_error(program->runtime, "Unsupported or invalid instruction.");
        }
        if (end-bc < required)
            return set_error(program->runtime, "Unexpected end of bytecode");
        bc += required;
    }
    
    if (!has_end_inst) return set_error(program->runtime, "No end instruction");
    else return true;
}

int get_property_index(const program_t* program, const char* name) {
    for (uint8_t i = 0; i < program->property_count; i++)
        if (strcmp(program->property_names[i], name) == 0)
            return program->property_indices[i];
    return -1;
}

size_t get_property_padding(const runtime_t* runtime) {
    return runtime->backend.get_property_padding(runtime);
}

bool create_system(system_t* system) {
    if (system->sim_program->type != PROGRAM_TYPE_SIMULATION)
        return set_error(system->runtime, "Simulation program is not a simulation program");
    
    size_t padding = get_property_padding(system->runtime);
    system->pool_size = ceil(system->pool_size/(double)padding) * padding;
    
    system->nexts = NULL;
    system->deleted_flags = NULL;
    memset(system->properties, 0, sizeof(system->properties));
    
    system->nexts = malloc(system->pool_size*sizeof(int));
    if (!system->nexts) return set_error(system->runtime, "Unable to allocate next indices");
    for (size_t i = 0; i < system->pool_size-1; i++)
        system->nexts[i] = i + 1;
    system->nexts[system->pool_size-1] = -1;
    system->next_particle = 0;
    
    system->deleted_flags = malloc(system->pool_size);
    if (!system->deleted_flags) return set_error(system->runtime, "Unable to allocate deleted flags");
    memset(system->deleted_flags, 1, system->pool_size);
    
    for (size_t i = 0; i < system->sim_program->property_count; i++) {
        int index = system->sim_program->property_indices[i];
        system->properties[index] = calloc(1, system->pool_size*sizeof(float));
        if (!system->properties[index]) {
            destroy_system(system);
            return set_error(system->runtime, "Unable to allocate property");
        }
    }
    
    return true;
}

bool destroy_system(system_t* system) {
    free(system->deleted_flags);
    free(system->nexts);
    for (size_t i = 0; i < system->sim_program->property_count; i++)
        free(system->properties[system->sim_program->property_indices[i]]);
    return true;
}

bool simulate_system(system_t* system) {
    return system->runtime->backend.simulate_system(system);
}

int spawn_particle(system_t* system) {
    int index = system->next_particle;
    if (index < 0) {
        set_error(system->runtime, "Pool is full");
        return -1;
    }
    
    system->next_particle = system->nexts[index];
    system->deleted_flags[index] = 0;
    
    return index;
}

bool delete_particle(system_t* system, int index) {
    if (index < 0 || index >= system->pool_size)
        return set_error(system->runtime, "Invalid particle index");
    
    system->deleted_flags[index] = 1;
    system->nexts[index] = system->next_particle;
    system->next_particle = index;
    
    return true;
}
