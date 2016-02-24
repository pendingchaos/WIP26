#ifndef AST_H
#define AST_H
#include <stddef.h>
#include <stdbool.h>

#include "shared.h"

typedef enum node_type_t {
    NODET_NUM,
    NODET_ID,
    NODET_ASSIGN,
    NODET_ADD,
    NODET_SUB,
    NODET_MUL,
    NODET_DIV,
    NODET_POW,
    NODET_MEMBER,
    NODET_VAR_DECL,
    NODET_PROP_DECL,
    NODET_RETURN,
    NODET_CALL,
    NODET_FUNC_DECL,
    NODET_NOP,
    NODET_NEG,
    NODET_DROP,
    NODET_IF
} node_type_t;

typedef struct node_t node_t;
typedef struct num_node_t num_node_t;
typedef struct id_node_t id_node_t;
typedef struct bin_node_t bin_node_t;
typedef struct unary_node_t unary_node_t;
typedef struct decl_node_t decl_node_t;
typedef struct call_node_t call_node_t;
typedef struct func_decl_node_t func_decl_node_t;
typedef struct if_node_t if_node_t;
typedef struct ast_t ast_t;

struct node_t {
    node_type_t type;
};

struct num_node_t {
    node_t head;
    double val;
};

struct id_node_t {
    node_t head;
    char* name;
};

struct bin_node_t {
    node_t head;
    node_t* lhs;
    node_t* rhs;
};

struct unary_node_t {
    node_t head;
    node_t* val;
};

struct decl_node_t {
    node_t head;
    char* name;
    char* dtype;
};

struct call_node_t {
    node_t head;
    char* func;
    size_t arg_count;
    node_t** args;
};

struct func_decl_node_t {
    node_t head;
    char* name;
    char* ret_type;
    size_t arg_count;
    size_t stmt_count;
    char** arg_names;
    char** arg_types;
    node_t** stmts;
};

struct if_node_t {
    node_t head;
    node_t* condition;
    size_t stmt_count;
    node_t** stmts;
};

struct ast_t {
    size_t stmt_count;
    node_t** stmts;
    char error[1024];
    mem_group_t* mem;
};

num_node_t* create_num_node(ast_t* ast, double val);
id_node_t* create_id_node(ast_t* ast, const char* name);
bin_node_t* create_bin_node(ast_t* ast, node_type_t type, node_t* lhs, node_t* rhs);
unary_node_t* create_unary_node(ast_t* ast, node_type_t type, node_t* val);
decl_node_t* create_decl_node(ast_t* ast, node_type_t type, const char* name, const char* dtype);
call_node_t* create_call_node(ast_t* ast, const char* func, size_t arg_count, node_t** args);
func_decl_node_t* create_func_decl_node(ast_t* ast, func_decl_node_t* decl);
if_node_t* create_if_node(ast_t* ast, size_t stmt_count, node_t** stmts, node_t* cond);
node_t* create_nop_node(ast_t* ast);
void free_node(node_t* node);
bool free_ast(ast_t* ast);
bool set_error(ast_t* ast, const char* format, ...);
bool validate_ast(const ast_t* ast);
bool add_drop_vars(ast_t* ast);
#endif
