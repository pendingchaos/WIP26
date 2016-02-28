#ifndef BC_H
#define BC_H
#include "ir.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BC_OP_ADD = 0,
    BC_OP_SUB = 1,
    BC_OP_MUL = 2,
    BC_OP_DIV = 3,
    BC_OP_POW = 4,
    BC_OP_MOVF = 5,
    BC_OP_SQRT = 6,
    BC_OP_LOAD_PROP = 7,
    BC_OP_STORE_PROP = 8,
    BC_OP_DELETE = 9,
    BC_OP_LESS = 10,
    BC_OP_GREATER = 11,
    BC_OP_EQUAL = 12,
    BC_OP_BOOL_AND = 13,
    BC_OP_BOOL_OR = 14,
    BC_OP_BOOL_NOT = 15,
    BC_OP_SEL = 16,
    BC_OP_COND_BEGIN = 17,
    BC_OP_COND_END = 18,
    BC_OP_END = 19
} bc_op_t;

typedef struct {
    ir_t* ir;
    
    bool simulation;
    size_t bc_size;
    uint8_t* bc;
    size_t prop_count;
    uint8_t* prop_indices;
    char error[1024];
} bc_t;

bool gen_bc(bc_t* bc, bool simulation);
bool write_bc(FILE* dest, const bc_t* bc);
void free_bc(bc_t* bc);
#endif
