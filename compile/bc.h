#ifndef BC_H
#define BC_H
#include "ir.h"

#include <stdio.h>
#include <stdbool.h>

bool write_bc(FILE* dest, const ir_t* ir, bool simulation);
char* bc_get_error();
#endif
