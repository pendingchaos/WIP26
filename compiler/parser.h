#ifndef PARSER_H
#define PARSER_H
#include <stdbool.h>
#include <stddef.h>

#include "ast.h"

bool parse_program(const char* src, ast_t* ast, size_t inc_dir_count, char** inc_dirs);
#endif
