#include "ast.h"
#include "shared.h"

#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

typedef enum dtype_t {
    DTYPE_ERROR,
    DTYPE_VOID,
    DTYPE_FLOAT,
    DTYPE_VEC2,
    DTYPE_VEC3,
    DTYPE_VEC4,
    DTYPE_BOOL,
    DTYPE_BVEC2,
    DTYPE_BVEC3,
    DTYPE_BVEC4,
} dtype_t;

typedef struct var_t {
    size_t count;
    char** names;
    
    dtype_t dtype;
} var_t;

typedef struct state_t {
    ast_t* ast;
    
    size_t func_count;
    func_decl_node_t** funcs;
    
    size_t var_count;
    var_t* vars;
    
    size_t bt_count;
    char** bt;
} state_t;

static var_t create_var_name(state_t* state, const char* name) {
    var_t var;
    var.count = state->bt_count + 1;
    var.names = alloc_mem(sizeof(char*)*var.count);
    for (size_t i = 0; i < state->bt_count; i++)
        var.names[i] = copy_str(state->bt[i]);
    var.names[state->bt_count] = copy_str(name);
    return var;
}

static void free_var(var_t* var) {
    for (size_t i = 0; i < var->count; i++) free(var->names[i]);
    free(var->names);
}

static bool var_name_equal(const var_t* a, const var_t* b) {
    if (a->count != b->count) return false;
    for (size_t i = 0; i < a->count; i++)
        if (strcmp(a->names[i], b->names[i]))
            return false;
    return true;
}

static dtype_t error(state_t* state, const node_t* node, const char* format, ...) {
    ast_t* ast = state->ast;
    
    snprintf(ast->error, sizeof(ast->error), "%u:%u: ", node->loc.line, node->loc.column);
    
    char* e = ast->error + strlen(ast->error);
    size_t ecap = sizeof(ast->error) - strlen(ast->error);
    
    va_list list;
    va_start(list, format);
    vsnprintf(e, ecap, format, list);
    va_end(list);
    
    return DTYPE_ERROR;
}

static dtype_t get_base_type(dtype_t type) {
    switch (type) {
    case DTYPE_ERROR: return DTYPE_ERROR;
    case DTYPE_VOID: return DTYPE_ERROR;
    case DTYPE_FLOAT: return DTYPE_FLOAT;
    case DTYPE_VEC2: return DTYPE_FLOAT;
    case DTYPE_VEC3: return DTYPE_FLOAT;
    case DTYPE_VEC4: return DTYPE_FLOAT;
    case DTYPE_BOOL: return DTYPE_BOOL;
    case DTYPE_BVEC2: return DTYPE_BOOL;
    case DTYPE_BVEC3: return DTYPE_BOOL;
    case DTYPE_BVEC4: return DTYPE_BOOL;
    default: assert(false);
    }
}

static size_t get_comp(dtype_t type) {
    switch (type) {
    case DTYPE_ERROR: return 0;
    case DTYPE_VOID: return 0;
    case DTYPE_FLOAT: return 1;
    case DTYPE_VEC2: return 2;
    case DTYPE_VEC3: return 3;
    case DTYPE_VEC4: return 4;
    case DTYPE_BOOL: return 1;
    case DTYPE_BVEC2: return 2;
    case DTYPE_BVEC3: return 3;
    case DTYPE_BVEC4: return 4;
    default: assert(false);
    }
}

static dtype_t str_as_dtype(state_t* state, const node_t* node, const char* s) {
    if (!strcmp(s, "void")) return DTYPE_VOID;
    if (!strcmp(s, "float")) return DTYPE_FLOAT;
    if (!strcmp(s, "vec2")) return DTYPE_VEC2;
    if (!strcmp(s, "vec3")) return DTYPE_VEC3;
    if (!strcmp(s, "vec4")) return DTYPE_VEC4;
    if (!strcmp(s, "bool")) return DTYPE_BOOL;
    if (!strcmp(s, "bvec2")) return DTYPE_BVEC2;
    if (!strcmp(s, "bvec3")) return DTYPE_BVEC3;
    if (!strcmp(s, "bvec4")) return DTYPE_BVEC4;
    return error(state, node, "Invalid data type.");
}

static dtype_t validate_node(state_t* state, const node_t* node, dtype_t ret_type) {
    switch (node->type) {
    case NODET_NUM: return DTYPE_FLOAT;
    case NODET_ID: {
        id_node_t* id = (id_node_t*)node;
        if (!strcmp(id->name, "true")) return DTYPE_BOOL;
        if (!strcmp(id->name, "false")) return DTYPE_BOOL;
        
        var_t var = create_var_name(state, id->name);
        for (size_t i = 0; i < state->var_count; i++)
            if (var_name_equal(state->vars+i, &var)) {
                free_var(&var);
                return state->vars[i].dtype;
            }
        
        free_var(&var);
        return error(state, node, "No such variable '%s'.", id->name);
    }
    case NODET_ASSIGN: {
        bin_node_t* bin = (bin_node_t*)node;
        
        dtype_t a = validate_node(state, bin->lhs, ret_type);
        if (a == DTYPE_ERROR) return DTYPE_ERROR;
        if (a == DTYPE_VOID) return error(state, bin->lhs, "Void value not ignored.");
        
        dtype_t b = validate_node(state, bin->rhs, ret_type);
        if (b == DTYPE_ERROR) return DTYPE_ERROR;
        if (b == DTYPE_VOID) return error(state, bin->rhs, "Void value not ignored.");
        
        if (a != b)
            return error(state, node, "Assignment with operands of incompatible types.");
        
        return DTYPE_VOID;
    }
    case NODET_ADD:
    case NODET_SUB:
    case NODET_MUL:
    case NODET_DIV:
    case NODET_POW:
    case NODET_LESS:
    case NODET_GREATER:
    case NODET_EQUAL: {
        bin_node_t* bin = (bin_node_t*)node;
        
        dtype_t a = validate_node(state, bin->lhs, ret_type);
        if (a == DTYPE_ERROR) return DTYPE_ERROR;
        if (get_base_type(a) == DTYPE_BOOL)
            return error(state, bin->lhs, "Incompatible data type for binary operation operand.");
        
        dtype_t b = validate_node(state, bin->rhs, ret_type);
        if (b == DTYPE_ERROR) return DTYPE_ERROR;
        if (get_base_type(b) == DTYPE_BOOL)
            return error(state, bin->rhs, "Incompatible data type for binary operation operand.");
        
        if (a != b) return error(state, node, "Incompatible operands for binary operation.");
        
        switch (node->type) {
        case NODET_LESS:
        case NODET_GREATER:
        case NODET_EQUAL:
            switch (get_comp(a)) {
            case 1: return DTYPE_BOOL;
            case 2: return DTYPE_BVEC2;
            case 3: return DTYPE_BVEC3;
            case 4: return DTYPE_BVEC4;
            }
        default:
            return a;
        }
    }
    case NODET_MEMBER: {
        bin_node_t* bin = (bin_node_t*)node;
        
        dtype_t a = validate_node(state, bin->lhs, ret_type);
        if (a == DTYPE_ERROR) return DTYPE_ERROR;
        dtype_t base = get_base_type(a);
        if (base == DTYPE_ERROR) return error(state, bin->lhs, "Invalid data type.");
        
        if (bin->rhs->type != NODET_ID) return error(state, bin->rhs, "Invalid swizzle.");
        const char* swizzle = ((id_node_t*)bin->rhs)->name;
        
        if (strlen(swizzle) > 4) return error(state, bin->rhs, "Invalid swizzle.");
        for (const char *c = swizzle; *c; c++)
            if (*c<'w' || *c>'z') return error(state, bin->rhs, "Invalid swizzle.");
        
        switch (strlen(swizzle)) {
        case 1: return base;
        case 2: return base==DTYPE_FLOAT ? DTYPE_VEC2 : DTYPE_BVEC2;
        case 3: return base==DTYPE_FLOAT ? DTYPE_VEC3 : DTYPE_BVEC3;
        case 4: return base==DTYPE_FLOAT ? DTYPE_VEC4 : DTYPE_BVEC4;
        }
    }
    case NODET_VAR_DECL:
    case NODET_ATTR_DECL:
    case NODET_UNI_DECL: {
        decl_node_t* decl = (decl_node_t*)node;
        
        var_t var = create_var_name(state, decl->name);
        var.dtype = str_as_dtype(state, node, decl->dtype);
        if (var.dtype == DTYPE_ERROR) return DTYPE_ERROR;
        if (var.dtype == DTYPE_VOID)
            return error(state, node, "Invalid data type.");
        
        for (size_t i = 0; i < state->var_count; i++)
            if (var_name_equal(&var, state->vars+i)) {
                free_var(&var);
                return error(state, node, "Redeclaration of '%s'", var.names[0]);
            }
        
        state->vars = append_mem(state->vars, state->var_count++, sizeof(var_t), &var);
        
        return var.dtype;
    }
    case NODET_RETURN: {
        if (ret_type == DTYPE_ERROR) return error(state, node, "Invalid return.");
        
        node_t* val = ((unary_node_t*)node)->val;
        
        dtype_t a = validate_node(state, val, ret_type);
        if (a == DTYPE_ERROR) return DTYPE_ERROR;
        if (a != ret_type) return error(state, val, "Incompatible return type.");
        
        return DTYPE_VOID;
    }
    case NODET_CALL: {
        call_node_t* call = (call_node_t*)node;
        
        dtype_t arg_types[call->arg_count];
        for (size_t i = 0; i < call->arg_count; i++) {
            arg_types[i] = validate_node(state, call->args[i], ret_type);
            if (arg_types[i] == DTYPE_ERROR) return DTYPE_ERROR;
        }
        
        //TODO: This is probably the ugliest code I've ever written
        if (!strcmp(call->func, "__sqrt") && call->arg_count==1 &&
            get_base_type(arg_types[0])==DTYPE_FLOAT) return arg_types[0];
        else if (!strcmp(call->func, "__sel") && call->arg_count==3 &&
                 arg_types[0]==arg_types[1] &&
                 get_comp(arg_types[0])==get_comp(arg_types[2]) &&
                 get_base_type(arg_types[2])==DTYPE_BOOL)
            return arg_types[0];
        else if (!strcmp(call->func, "__del") && call->arg_count==0)
            return DTYPE_VOID;
        
        for (size_t i = 0; i < state->func_count; i++)
            if (!strcmp(state->funcs[i]->name, call->func)) {
                func_decl_node_t* func = state->funcs[i];
                
                if (call->arg_count != func->arg_count) continue;
                
                bool compatible = true;
                for (size_t j = 0; j < func->arg_count; j++)
                    if (str_as_dtype(state, (node_t*)func, func->arg_types[j]) != arg_types[j])
                        compatible = false;
                if (!compatible) continue;
                
                return str_as_dtype(state, (node_t*)func, func->ret_type);
            }
        
        return error(state, node, "Unable to find compatible overload for '%s'.", call->func);
    }
    case NODET_FUNC_DECL: {
        func_decl_node_t* decl = (func_decl_node_t*)node;
        
        dtype_t rtype = str_as_dtype(state, node, decl->ret_type);
        if (rtype == DTYPE_ERROR) return DTYPE_ERROR;
        
        char* name = alloc_mem(17);
        snprintf(name, 17, "%zx", (size_t)decl);
        
        state->bt = append_mem(state->bt, state->bt_count++, sizeof(char*), &name);
        
        bool error = false;
        
        for (size_t i = 0; i < decl->arg_count; i++) {
            dtype_t dtype = str_as_dtype(state, node, decl->arg_types[i]);
            if (dtype == DTYPE_ERROR) {
                error = true;
                goto end;
            }
            
            var_t var = create_var_name(state, decl->arg_names[i]);
            var.dtype = dtype;
            state->vars = append_mem(state->vars, state->var_count++, sizeof(var_t), &var);
        }
        
        for (size_t i = 0; i < decl->stmt_count; i++)
            if (validate_node(state, decl->stmts[i], rtype) == DTYPE_ERROR) {
                error = true;
                goto end;
            }
        
        end:
            free(state->bt[state->bt_count-1]);
            state->bt = realloc_mem(state->bt, sizeof(char*)*state->bt_count--);
            if (error) return DTYPE_ERROR;
        
        state->funcs = append_mem(state->funcs, state->func_count++, sizeof(decl_node_t*), &decl);
        return DTYPE_VOID;
    }
    case NODET_NEG:
    case NODET_BOOL_NOT: {
        node_t* val = ((unary_node_t*)node)->val;
        
        dtype_t a = validate_node(state, val, ret_type);
        if (a == DTYPE_ERROR) return DTYPE_ERROR;
        if (get_base_type(a) == (node->type==NODET_NEG ?DTYPE_BOOL : DTYPE_FLOAT))
            return error(state, val, "Invalid data type for unary operation.");
        
        return a;
    }
    case NODET_IF:
    case NODET_WHILE: {
        cond_node_t* cond = (cond_node_t*)node;
        
        dtype_t condt = validate_node(state, cond->condition, ret_type);
        if (condt == DTYPE_ERROR) return DTYPE_ERROR;
        if (condt != DTYPE_BOOL)
            return error(state, cond->condition, "Condition must evaluate to a boolean");
        
        for (size_t i = 0; i < cond->stmt_count; i++)
            if (validate_node(state, cond->stmts[i], ret_type) == DTYPE_ERROR)
                return DTYPE_ERROR;
        
        return DTYPE_VOID;
    }
    case NODET_BOOL_AND:
    case NODET_BOOL_OR: {
        bin_node_t* bin = (bin_node_t*)node;
        
        dtype_t a = validate_node(state, bin->lhs, ret_type);
        if (a == DTYPE_ERROR) return DTYPE_ERROR;
        if (get_base_type(a) == DTYPE_FLOAT)
            return error(state, bin->lhs, "Incompatible data type for binary operation operand.");
        
        dtype_t b = validate_node(state, bin->rhs, ret_type);
        if (b == DTYPE_ERROR) return DTYPE_ERROR;
        if (get_base_type(b) == DTYPE_FLOAT)
            return error(state, bin->rhs, "Incompatible data type for binary operation operand.");
        
        return a;
    }
    case NODET_NOP:
    case NODET_DROP: {
        return DTYPE_VOID;
    }
    default: {
        return error(state, node, "Unreachable code at %s:%u.", __FILE__, __LINE__);
    }
    }
}

bool validate_ast(ast_t* ast) {
    state_t state;
    memset(&state, 0, sizeof(state_t));
    state.ast = ast;
    
    bool res = true;
    for (size_t i = 0; i < ast->stmt_count; i++)
        if (validate_node(&state, ast->stmts[i], DTYPE_ERROR) == DTYPE_ERROR) {
            res = false;
            break;
        }
    
    free(state.bt);
    for (size_t i = 0; i < state.var_count; i++)
        free_var(state.vars+i);
    free(state.vars);
    free(state.funcs);
    
    return res;
}
