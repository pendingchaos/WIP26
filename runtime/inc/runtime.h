#ifndef RUNTIME_H
#define RUNTIME_H
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum bc_op_t {
    BC_OP_ADD_R_F = 0,
    BC_OP_ADD_RR = 1,
    BC_OP_SUB_R_F = 2,
    BC_OP_SUB_F_R = 3,
    BC_OP_SUB_RR = 4,
    BC_OP_MUL_R_F = 5,
    BC_OP_MUL_RR = 6,
    BC_OP_DIV_R_F = 7,
    BC_OP_DIV_F_R = 8,
    BC_OP_DIV_RR = 9,
    BC_OP_POW_R_F = 10,
    BC_OP_POW_F_R = 11,
    BC_OP_POW_RR = 12,
    BC_OP_MOV_F = 13,
    BC_OP_MIN_RR = 14,
    BC_OP_MIN_R_F = 15,
    BC_OP_MAX_RR = 16,
    BC_OP_MAX_R_F = 17,
    BC_OP_SQRT = 18,
    BC_OP_LOAD_PROP = 19,
    BC_OP_STORE_PROP = 20,
    BC_OP_DELETE = 21,
    BC_OP_LESS = 22,
    BC_OP_GREATER = 23,
    BC_OP_EQUAL = 24,
    BC_OP_BOOL_AND = 25,
    BC_OP_BOOL_OR = 26,
    BC_OP_BOOL_NOT = 27,
    BC_OP_COND_BEGIN = 28,
    BC_OP_COND_END = 29,
    BC_OP_END = 30
} bc_op_t;

typedef enum program_type_t {
    PROGRAM_TYPE_SIMULATION = 0,
    PROGRAM_TYPE_EMITTER = 1
} program_type_t;

typedef struct runtime_t runtime_t;
typedef struct backend_t backend_t;
typedef struct program_t program_t;
typedef struct system_t system_t;

struct backend_t {
    bool (*create)(runtime_t* runtime);
    bool (*destroy)(runtime_t* runtime);
    size_t (*get_property_padding)(const runtime_t* runtime);
    bool (*create_program)(program_t* program);
    bool (*destroy_program)(program_t* program);
    bool (*simulate_system)(system_t* program);
    void* internal;
};

struct runtime_t {
    char error[256];
    backend_t backend;
};

struct program_t {
    runtime_t* runtime;
    program_type_t type;
    uint8_t property_count;
    char* property_names[256];
    uint8_t property_indices[256];
    uint32_t bc_size;
    uint8_t* bc;
    
    void* backend_internal;
};

struct system_t {
    runtime_t* runtime;
    program_t* sim_program;
    size_t pool_size;
    int* nexts;
    int next_particle;
    
    float* properties[256];
    uint8_t* deleted_flags;
};

bool create_runtime(runtime_t* runtime, const char* backend);
bool destroy_runtime(runtime_t* runtime);
size_t get_property_padding(const runtime_t* runtime);
bool set_error(runtime_t* runtime, const char* message);

bool open_program(const char* filename, program_t* program);
bool destroy_program(program_t* program);
bool validate_program(const program_t* program);
int get_property_index(const program_t* program, const char* name);

bool create_system(system_t* system);
bool destroy_system(system_t* system);
bool simulate_system(system_t* system);
int spawn_particle(system_t* system);
bool delete_particle(system_t* system, int index);
#endif
