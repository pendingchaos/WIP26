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
    BC_OP_END = 17
} bc_op_t;

typedef enum program_type_t {
    PROGRAM_TYPE_SIMULATION = 0,
    PROGRAM_TYPE_EMITTER = 1
} program_type_t;

typedef enum prop_dtype_t {
    PROP_UINT8 = 0,
    PROP_INT8 = 1,
    PROP_UINT16 = 2,
    PROP_INT16 = 3,
    PROP_UINT32 = 4,
    PROP_INT32 = 5,
    PROP_FLOAT32 = 6,
    PROP_FLOAT64 = 7
} prop_dtype_t;

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
    uint8_t property_load_regs[256];
    uint8_t property_store_regs[256];
    uint32_t bc_size;
    uint8_t* bc;
    
    void* backend_internal;
};

struct system_t {
    runtime_t* runtime;
    program_t* sim_program;
    size_t pool_size;
    size_t pool_usage;
    int* nexts;
    int next_particle;
    
    void* properties[256];
    prop_dtype_t property_dtypes[256];
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
