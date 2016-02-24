#include "ast.h"
#include "parser.h"
#include "shared.h"

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

num_node_t* create_num_node(ast_t* ast, double val) {
    num_node_t* node = alloc_mem(sizeof(num_node_t));
    node->head.type = NODET_NUM;
    node->val = val;
    return node;
}

id_node_t* create_id_node(ast_t* ast, const char* name) {
    id_node_t* node = alloc_mem(sizeof(id_node_t));
    node->head.type = NODET_ID;
    node->name = alloc_mem(strlen(name)+1);
    strcpy(node->name, name);
    return node;
}

bin_node_t* create_bin_node(ast_t* ast, node_type_t type, node_t* lhs, node_t* rhs) {
    bin_node_t* node = alloc_mem(sizeof(bin_node_t));
    node->head.type = type;
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

unary_node_t* create_unary_node(ast_t* ast, node_type_t type, node_t* val) {
    unary_node_t* node = alloc_mem(sizeof(unary_node_t));
    node->head.type = type;
    node->val = val;
    return node;
}

decl_node_t* create_decl_node(ast_t* ast, node_type_t type, const char* name, const char* dtype) {
    decl_node_t* node = alloc_mem(sizeof(decl_node_t));
    node->head.type = type;
    node->name = malloc(strlen(name)+1);
    node->dtype = alloc_mem(strlen(dtype)+1);
    strcpy(node->name, name);
    strcpy(node->dtype, dtype);
    return node;
}

call_node_t* create_call_node(ast_t* ast, const char* func, size_t arg_count, node_t** args) {
    call_node_t* node = alloc_mem(sizeof(call_node_t));
    node->head.type = NODET_CALL;
    node->func = alloc_mem(strlen(func)+1);
    node->args = alloc_mem(arg_count*sizeof(node_t*));
    node->arg_count = arg_count;
    memcpy(node->args, args, arg_count*sizeof(node_t*));
    strcpy(node->func, func);
    return node;
}

func_decl_node_t* create_func_decl_node(ast_t* ast, func_decl_node_t* decl) {
    func_decl_node_t* node = alloc_mem(sizeof(func_decl_node_t));
    node->head.type = NODET_FUNC_DECL;
    node->name = NULL;
    node->arg_names = NULL;
    node->arg_types = NULL;
    node->ret_type = NULL;
    node->name = alloc_mem(strlen(decl->name)+1);
    node->ret_type = alloc_mem(strlen(decl->ret_type)+1);
    node->arg_names = alloc_mem(decl->arg_count*sizeof(char*));
    node->arg_types = alloc_mem(decl->arg_count*sizeof(char*));
    node->stmts = alloc_mem(decl->stmt_count*sizeof(node_t*));
    strcpy(node->ret_type, decl->ret_type);
    strcpy(node->name, decl->name);
    memcpy(node->arg_names, decl->arg_names, decl->arg_count*sizeof(char*));
    memcpy(node->arg_types, decl->arg_types, decl->arg_count*sizeof(char*));
    memcpy(node->stmts, decl->stmts, decl->stmt_count*sizeof(node_t*));
    node->stmt_count = decl->stmt_count;
    node->arg_count = decl->arg_count;
    return node;
}

node_t* create_nop_node(ast_t* ast) {
    node_t* node = alloc_mem(sizeof(node_t));
    node->type = NODET_NOP;
    return node;
}

void free_node(node_t* node) {
    if (!node) return;
    switch (node->type) {
    case NODET_NUM:
    case NODET_NOP: break;
    case NODET_ID: free(((id_node_t*)node)->name); break;
    case NODET_ASSIGN:
    case NODET_ADD:
    case NODET_SUB:
    case NODET_MUL:
    case NODET_DIV:
    case NODET_POW:
    case NODET_MEMBER: {
        free_node(((bin_node_t*)node)->lhs);
        free_node(((bin_node_t*)node)->rhs);
        break;
    }
    case NODET_VAR_DECL:
    case NODET_PROP_DECL: {
        free(((decl_node_t*)node)->name);
        free(((decl_node_t*)node)->dtype);
        break;
    }
    case NODET_DROP:
    case NODET_RETURN:
    case NODET_NEG: {
        free_node(((unary_node_t*)node)->val);
        break;
    }
    case NODET_CALL: {
        call_node_t* call = (call_node_t*)node;
        free(call->func);
        for (size_t i = 0; i < call->arg_count; i++)
            free_node(call->args[i]);
        free(call->args);
        break;
    }
    case NODET_FUNC_DECL: {
        func_decl_node_t* decl = (func_decl_node_t*)node;
        free(decl->name);
        free(decl->ret_type);
        for (size_t i = 0; i < decl->arg_count; i++) free(decl->arg_names[i]);
        for (size_t i = 0; i < decl->arg_count; i++) free(decl->arg_types[i]);
        for (size_t i = 0; i < decl->stmt_count; i++)
            free_node(decl->stmts[i]);
        free(decl->arg_names);
        free(decl->arg_types);
        free(decl->stmts);
        break;
    }
    }
    free(node);
}

bool free_ast(ast_t* ast) {
    for (size_t i = 0; i < ast->stmt_count; i++)
        free_node(ast->stmts[i]);
    free(ast->stmts);
    ast->stmt_count = 0;
    ast->stmts = NULL;
    return true;
}

bool set_error(ast_t* ast, const char* format, ...) {
    va_list list;
    va_start(list, format);
    vsnprintf(ast->error, sizeof(ast->error), format, list);
    va_end(list);
    return false;
}

bool validate_ast(const ast_t* ast) {
    return true;
}

static bool get_vars(ast_t* ast, size_t node_count, node_t** nodes, size_t* var_count, id_node_t** vars) {
    for (size_t i = 0; i < node_count; i++) {
        node_t* node = nodes[i];
        switch (node->type) {
        case NODET_ID: {
            id_node_t* id = (id_node_t*)node;
            for (size_t j = 0; j < *var_count; j++)
                if (!strcmp((*vars)[j].name, id->name))
                    return true;
            
            *vars = append_mem(*vars, (*var_count)++, sizeof(id_node_t), id);
            break;
        }
        case NODET_ASSIGN: {
            node_t* lhs = ((bin_node_t*)node)->lhs;
            while (lhs->type == NODET_MEMBER) lhs = ((bin_node_t*)lhs)->lhs;
            get_vars(ast, 1, &lhs, var_count, vars);
            break;
        }
        case NODET_ADD:
        case NODET_SUB:
        case NODET_MUL:
        case NODET_DIV:
        case NODET_POW: {
            bin_node_t* bin = (bin_node_t*)node;
            get_vars(ast, 1, &bin->lhs, var_count, vars);
            get_vars(ast, 1, &bin->rhs, var_count, vars);
            break;
        }
        case NODET_MEMBER: {
            bin_node_t* bin = (bin_node_t*)node;
            get_vars(ast, 1, &bin->lhs, var_count, vars);
            break;
        }
        case NODET_NEG:
        case NODET_DROP:
        case NODET_RETURN: {
            unary_node_t* unary = (unary_node_t*)node;
            get_vars(ast, 1, &unary->val, var_count, vars);
            break;
        }
        case NODET_CALL: {
            call_node_t* call = (call_node_t*)node;
            for (size_t i = 0; i < call->arg_count; i++)
                get_vars(ast, 1, &call->args[i], var_count, vars);
        }
        default: continue;
        }
    }
    
    return true;
}

static void _add_drop_vars(ast_t* ast, size_t count, node_t** nodes, size_t* res_count, node_t*** res_nodes) {
    size_t new_count = 0;
    node_t** new_nodes = NULL;
    
    size_t var_count = 0;
    id_node_t* vars = NULL;
    for (size_t i = 0; i < count; i++) {
        new_nodes = append_mem(new_nodes, new_count++, sizeof(node_t*), &nodes[i]);
        
        if (nodes[i]->type == NODET_FUNC_DECL) {
            func_decl_node_t* decl = (func_decl_node_t*)nodes[i];
            size_t stmt_count = 0;
            node_t** stmts = NULL;
            _add_drop_vars(ast, decl->stmt_count, decl->stmts, &stmt_count, &stmts);
            free(decl->stmts);
            decl->stmt_count = stmt_count;
            decl->stmts = stmts;
        }
        
        size_t future_count = 0;
        id_node_t* future = NULL;
        get_vars(ast, count-i-1, nodes+i+1, &future_count, &future);
        
        get_vars(ast, 1, nodes+i, &var_count, &vars);
        
        for (ptrdiff_t j = 0; j < var_count; j++) {
            for (size_t k = 0; k < future_count; k++)
                if (!strcmp(vars[j].name, future[k].name))
                    goto end;
            
            id_node_t* id = create_id_node(ast, vars[j].name);
            node_t* node = (node_t*)create_unary_node(ast, NODET_DROP, (node_t*)id);
            new_nodes = append_mem(new_nodes, new_count++, sizeof(node_t*), &node);
            
            memmove(vars+j, vars+j+1, (var_count-j-1)*sizeof(id_node_t));
            if (var_count-1)
                vars = realloc_mem(vars, (var_count-1)*sizeof(id_node_t));
            else {
                free(vars);
                vars = NULL;
            }
            j--;
            var_count--;
            
            end: ;
        }
        
        free(future);
    }
    free(vars);
    *res_count = new_count;
    *res_nodes = new_nodes;
}

bool add_drop_vars(ast_t* ast) {
    size_t res_count = 0;
    node_t** res_nodes;
    _add_drop_vars(ast, ast->stmt_count, ast->stmts, &res_count, &res_nodes);
    free(ast->stmts);
    ast->stmt_count = res_count;
    ast->stmts = res_nodes;
    return true;
}
