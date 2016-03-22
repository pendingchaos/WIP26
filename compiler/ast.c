#include "ast.h"
#include "parser.h"
#include "shared.h"

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

num_node_t* create_num_node(ast_t* ast, src_loc_t loc, double val) {
    num_node_t* node = mem_group_alloc(ast->mem, sizeof(num_node_t));
    node->head.type = NODET_NUM;
    node->head.loc = loc;
    node->val = val;
    return node;
}

id_node_t* create_id_node(ast_t* ast, src_loc_t loc, const char* name) {
    id_node_t* node = mem_group_alloc(ast->mem, sizeof(id_node_t));
    node->head.type = NODET_ID;
    node->head.loc = loc;
    node->name = copy_str_group(ast->mem, name);
    return node;
}

bin_node_t* create_bin_node(ast_t* ast, src_loc_t loc, node_type_t type, node_t* lhs, node_t* rhs) {
    bin_node_t* node = mem_group_alloc(ast->mem, sizeof(bin_node_t));
    node->head.type = type;
    node->head.loc = loc;
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

unary_node_t* create_unary_node(ast_t* ast, src_loc_t loc, node_type_t type, node_t* val) {
    unary_node_t* node = mem_group_alloc(ast->mem, sizeof(unary_node_t));
    node->head.type = type;
    node->head.loc = loc;
    node->val = val;
    return node;
}

decl_node_t* create_decl_node(ast_t* ast, src_loc_t loc, node_type_t type, const char* name, const char* dtype) {
    decl_node_t* node = mem_group_alloc(ast->mem, sizeof(decl_node_t));
    node->head.type = type;
    node->head.loc = loc;
    node->name = copy_str_group(ast->mem, name);
    node->dtype = copy_str_group(ast->mem, dtype);
    return node;
}

call_node_t* create_call_node(ast_t* ast, src_loc_t loc, const char* func, size_t arg_count, node_t** args) {
    call_node_t* node = mem_group_alloc(ast->mem, sizeof(call_node_t));
    node->head.type = NODET_CALL;
    node->head.loc = loc;
    node->func = copy_str_group(ast->mem, func);
    node->args = mem_group_alloc(ast->mem, arg_count*sizeof(node_t*));
    node->arg_count = arg_count;
    memcpy(node->args, args, arg_count*sizeof(node_t*));
    return node;
}

func_decl_node_t* create_func_decl_node(ast_t* ast, src_loc_t loc, func_decl_node_t* decl) {
    func_decl_node_t* node = mem_group_alloc(ast->mem, sizeof(func_decl_node_t));
    node->head.type = NODET_FUNC_DECL;
    node->head.loc = loc;
    node->name = NULL;
    node->arg_names = NULL;
    node->arg_types = NULL;
    node->ret_type = NULL;
    node->name = copy_str_group(ast->mem, decl->name);
    node->ret_type = copy_str_group(ast->mem, decl->ret_type);
    node->arg_names = mem_group_alloc(ast->mem, decl->arg_count*sizeof(char*));
    node->arg_types = mem_group_alloc(ast->mem, decl->arg_count*sizeof(char*));
    node->stmts = mem_group_alloc(ast->mem, decl->stmt_count*sizeof(node_t*));
    for (size_t i = 0; i < decl->arg_count; i++) {
        node->arg_names[i] = copy_str_group(ast->mem, decl->arg_names[i]);
        node->arg_types[i] = copy_str_group(ast->mem, decl->arg_types[i]);
    }
    memcpy(node->stmts, decl->stmts, decl->stmt_count*sizeof(node_t*));
    node->stmt_count = decl->stmt_count;
    node->arg_count = decl->arg_count;
    return node;
}

cond_node_t* create_cond_node(ast_t* ast, src_loc_t loc, node_type_t type, size_t stmt_count, node_t** stmts, node_t* cond) {
    cond_node_t* node = mem_group_alloc(ast->mem, sizeof(cond_node_t));
    node->head.type = type;
    node->head.loc = loc;
    node->stmts = mem_group_alloc(ast->mem, stmt_count*sizeof(node_t*));
    memcpy(node->stmts, stmts, stmt_count*sizeof(node_t*));
    node->condition = cond;
    node->stmt_count = stmt_count;
    return node;
}

node_t* create_nop_node(ast_t* ast) {
    node_t* node = mem_group_alloc(ast->mem, sizeof(node_t));
    node->type = NODET_NOP;
    return node;
}

bool free_ast(ast_t* ast) {
    destroy_mem_group(ast->mem);
    free(ast->stmts);
    return true;
}

bool set_error(ast_t* ast, const char* format, ...) {
    va_list list;
    va_start(list, format);
    vsnprintf(ast->error, sizeof(ast->error), format, list);
    va_end(list);
    return false;
}
