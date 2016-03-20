#ifndef BC_H
#define BC_H
#include "ir.h"
#include "shared.h"

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
    BC_OP_RAND = 22
} bc_op_t;

typedef struct {
    ir_t* ir;
    prog_type_t ptype;
    
    size_t bc_size;
    uint8_t* bc;
    uint8_t* attr_load_regs;
    uint8_t* attr_store_regs;
    uint8_t* uni_regs;
    char error[1024];
} bc_t;

bool gen_bc(bc_t* bc);
bool write_bc(FILE* dest, const bc_t* bc);
void free_bc(bc_t* bc);
#endif
