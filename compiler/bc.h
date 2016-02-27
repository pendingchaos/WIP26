#ifndef BC_H
#define BC_H
#include "ir.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

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
