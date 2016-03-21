#ifndef RUNTIME_H
#define RUNTIME_H
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum bc_op_t {
    BC_OP_ADD = 0,
    BC_OP_SUB = 1,
    BC_OP_MUL = 2,
    BC_OP_DIV = 3,
    BC_OP_POW = 4,
    BC_OP_MOVF = 5,
    BC_OP_SQRT = 6,
    BC_OP_DELETE = 7,
    BC_OP_LESS = 8,
    BC_OP_GREATER = 9,
    BC_OP_EQUAL = 10,
    BC_OP_BOOL_AND = 11,
    BC_OP_BOOL_OR = 12,
    BC_OP_BOOL_NOT = 13,
    BC_OP_SEL = 14,
    BC_OP_COND_BEGIN = 15,
    BC_OP_COND_END = 16,
    BC_OP_WHILE_BEGIN = 17,
    BC_OP_WHILE_END_COND = 18,
    BC_OP_WHILE_END = 19,
    BC_OP_END = 20,
    BC_OP_EMIT = 21,
    BC_OP_RAND = 22,
    BC_OP_FLOOR = 23
} bc_op_t;

typedef enum program_type_t {
    PROGRAM_TYPE_SIMULATION = 0,
    PROGRAM_TYPE_EMITTER = 1
} program_type_t;

typedef enum attr_dtype_t {
    ATTR_UINT8 = 0,
    ATTR_INT8 = 1,
    ATTR_UINT16 = 2,
    ATTR_INT16 = 3,
    ATTR_UINT32 = 4,
    ATTR_INT32 = 5,
    ATTR_FLOAT32 = 6,
    ATTR_FLOAT64 = 7
} attr_dtype_t;

typedef struct runtime_t runtime_t;
typedef struct backend_t backend_t;
typedef struct program_t program_t;
typedef struct particles_t particles_t;
typedef struct system_t system_t;

struct backend_t {
    bool (*create)(runtime_t* runtime);
    bool (*destroy)(runtime_t* runtime);
    size_t (*get_attribute_padding)(const runtime_t* runtime);
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
    uint8_t attribute_count;
    char* attribute_names[256];
    uint8_t attribute_load_regs[256];
    uint8_t attribute_store_regs[256];
    uint8_t uniform_count;
    char* uniform_names[256];
    uint8_t uniform_regs[256];
    uint32_t bc_size;
    uint8_t* bc;
    
    void* backend_internal;
};

struct particles_t {
    runtime_t* runtime;
    
    size_t pool_size;
    size_t pool_usage;
    int* nexts;
    int next_particle;
    
    char* attribute_names[256];
    attr_dtype_t attribute_dtypes[256];
    void* attributes[256];
    uint8_t* deleted_flags;
};

struct system_t {
    runtime_t* runtime;
    program_t* sim_program;
    program_t* emit_program;
    
    particles_t* particles;
    
    float sim_uniforms[256];
    uint8_t sim_attribute_indices[256];
    
    float emit_uniforms[256];
    uint8_t emit_attribute_indices[256];
};

bool create_runtime(runtime_t* runtime, const char* backend);
bool destroy_runtime(runtime_t* runtime);
size_t get_attribute_padding(const runtime_t* runtime);
bool set_error(runtime_t* runtime, const char* message);

bool open_program(const char* filename, program_t* program);
bool destroy_program(program_t* program);
bool validate_program(const program_t* program);
int get_attribute_index(const program_t* program, const char* name);
int get_uniform_index(const program_t* program, const char* name);

bool create_particles(particles_t* particles, size_t pool_size);
bool destroy_particles(particles_t* particles);
bool add_attribute(particles_t* particles, const char* name, attr_dtype_t dtype, int* index);

bool create_system(system_t* system);
bool destroy_system(system_t* system);
bool simulate_system(system_t* system);
int spawn_particle(particles_t* particles);
bool delete_particle(particles_t* particles, int index);
#endif
