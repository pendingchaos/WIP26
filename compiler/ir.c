#include "ir.h"
#include "ast.h"
#include "shared.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static bool is_valid_comp(char c, size_t comp) {
    if (c=='x' && comp>=1) return true;
    else if (c=='y' && comp>=2) return true;
    else if (c=='z' && comp>=3) return true;
    else if (c=='w' && comp>=4) return true;
    else return false;
}

static size_t get_comp_from_c(char c) {
    if (c=='x') return 0;
    else if (c=='y') return 1;
    else if (c=='z') return 2;
    else if (c=='w') return 3;
    else return 0;
}

static bool ir_set_error(ir_t* ir, const char* format, ...) {
    va_list list;
    va_start(list, format);
    vsnprintf(ir->error, sizeof(ir->error), format, list);
    va_end(list);
    return false;
}

static size_t get_dtype_comp(const char* dtype) {
    if (!strcmp(dtype, "float")) return 1;
    else if (!strcmp(dtype, "bool")) return 1;
    else if (!strcmp(dtype, "bvec2")) return 2;
    else if (!strcmp(dtype, "bvec3")) return 3;
    else if (!strcmp(dtype, "bvec4")) return 4;
    else if (!strcmp(dtype, "vec2")) return 2;
    else if (!strcmp(dtype, "vec3")) return 3;
    else if (!strcmp(dtype, "vec4")) return 4;
    else if (!strcmp(dtype, "void")) return 0;
    return 0;
}

static ir_inst_t* add_inst(ir_t* ir, ir_inst_t* inst) {
    ir->insts = append_mem(ir->insts, ir->inst_count, sizeof(ir_inst_t), inst);
    ir_inst_t* res = ir->insts+(ir->inst_count++);
    res->id = ir->next_inst_id++;
    return res;
}

static ir_operand_t create_num_operand(double num) {
    ir_operand_t res;
    res.type = IR_OPERAND_NUM;
    res.num = num;
    return res;
}

static ir_operand_t create_var_operand(ir_var_t var) {
    ir_operand_t res;
    res.type = IR_OPERAND_VAR;
    res.var = var;
    return res;
}

static ir_operand_t create_prop_operand(unsigned int index, unsigned int comp) {
    ir_operand_t res;
    res.type = IR_OPERAND_PROP;
    res.prop.index = index;
    res.prop.comp = comp;
    return res;
}

static bool var_name_equal(ir_var_name_t a, ir_var_name_t b) {
    if (a.func_count != b.func_count) return false;
    if (strcmp(a.name, b.name) != 0) return false;
    for (size_t i = 0; i < a.func_count; i++)
        if (strcmp(a.funcs[i], b.funcs[i]) != 0) return false;
    return true;
}

static ir_var_decl_t* decl_var(ir_t* ir, char* name, size_t comp, size_t func_count, char** funcs) {
    ir_var_name_t var_name;
    var_name.func_count = func_count;
    var_name.funcs = funcs;
    var_name.name = name;
    for (size_t i = 0; i < ir->var_count; i++)
        if (var_name_equal(var_name, ir->vars[i]->name))
            return ir->vars[i];
    
    ir_var_decl_t* var = alloc_mem(sizeof(ir_var_decl_t));
    var->name.func_count = func_count;
    var->name.funcs = alloc_mem(sizeof(char*)*func_count);
    for (size_t i = 0; i < func_count; i++)
        var->name.funcs[i] = copy_str(funcs[i]);
    var->name.name = copy_str(name);
    var->comp = comp;
    for (size_t i = 0; i < 4; i++) var->current_ver[i] = 0;
    
    ir->vars = append_mem(ir->vars, ir->var_count++, sizeof(ir_var_decl_t*), &var);
    
    return var;
}

static ir_var_decl_t* gen_temp_var(ir_t* ir, size_t comp) {
    char name[16];
    snprintf(name, sizeof(name), "~irtmp%u", ir->next_temp_var++);
    
    return decl_var(ir, name, comp, 0, NULL);
}

static ir_var_decl_t* get_id_var_decl(ir_t* ir, id_node_t* id, size_t func_count, char** funcs) {
    ir_var_name_t name;
    
    name.func_count = func_count;
    name.funcs = funcs;
    name.name = id->name;
    
    ir_var_decl_t* res = NULL;
    for (size_t i = 0; i < ir->var_count; i++)
        if (var_name_equal(ir->vars[i]->name, name)) {
            res = ir->vars[i];
            break;
        }
    
    return res;
}

static ir_var_t get_var_comp(ir_var_decl_t* decl, size_t comp_idx) {
    ir_var_t var;
    var.decl = decl;
    var.ver = decl->current_ver[comp_idx];
    var.comp_idx = comp_idx;
    return var;
}

static bool is_builtin_func(call_node_t* call, ir_var_decl_t** args) {
    if (!strcmp(call->func, "__sqrt") && call->arg_count==1) return true;
    else if (!strcmp(call->func, "__sel") && call->arg_count==3 && args[0]->comp==args[1]->comp && args[2]->comp==args[0]->comp) return true;
    else if (!strcmp(call->func, "__del") && call->arg_count==0) return true;
    return false;
}

static ir_var_decl_t* gen_builtin_func(ir_t* ir, call_node_t* call, ir_var_decl_t** args) {
    if (!strcmp(call->func, "__sqrt")) {
        ir_var_decl_t* dest = gen_temp_var(ir, args[0]->comp);
        ir_inst_t inst;
        inst.op = IR_OP_SQRT;
        inst.operand_count = 2;
        for (size_t i = 0; i < args[0]->comp; i++) {
            inst.operands[0] = create_var_operand(get_var_comp(dest, i));
            inst.operands[1] = create_var_operand(get_var_comp(args[0], i));
            add_inst(ir, &inst);
        }
        return dest;
    } else if (!strcmp(call->func, "__sel")) {
        ir_var_decl_t* dest = gen_temp_var(ir, args[0]->comp);
        ir_inst_t inst;
        inst.op = IR_OP_SEL;
        inst.operand_count = 4;
        for (size_t i = 0; i < args[0]->comp; i++) {
            inst.operands[0] = create_var_operand(get_var_comp(dest, i));
            inst.operands[1] = create_var_operand(get_var_comp(args[0], i));
            inst.operands[2] = create_var_operand(get_var_comp(args[1], i));
            inst.operands[3] = create_var_operand(get_var_comp(args[2], i));
            add_inst(ir, &inst);
        }
        return dest;
    } else if (!strcmp(call->func, "__del")) {
        ir_inst_t inst;
        inst.op = IR_OP_DELETE;
        inst.operand_count = 0;
        add_inst(ir, &inst);
        return gen_temp_var(ir, 0);
    }
    
    assert(false);
}

static ir_var_decl_t* node_to_ir(node_t* node, ir_t* ir, bool* returned, size_t func_count, char** funcs);

static ir_var_decl_t* gen_func_call(ir_t* ir, call_node_t* call, size_t func_count, char** funcs) {
    ir_var_decl_t** args = alloc_mem(call->arg_count*sizeof(ir_var_decl_t*));
    for (size_t i = 0; i < call->arg_count; i++) {
        bool _;
        ir_var_decl_t* arg = node_to_ir(call->args[i], ir, &_, func_count, funcs);
        if (!arg) {
            free(args);
            return NULL;
        }
        args[i] = arg;
    }
    
    if (is_builtin_func(call, args)) {
        ir_var_decl_t* res = gen_builtin_func(ir, call, args);
        free(args);
        return res;
    }
    
    func_decl_node_t* func = NULL;
    for (size_t i = 0; i < ir->func_count; i++)
        if (!strcmp(ir->funcs[i]->name, call->func) &&
            ir->funcs[i]->arg_count==call->arg_count) {
            bool compat = true;
            for (size_t j = 0; j < call->arg_count; j++)
                if (args[j]->comp != get_dtype_comp(ir->funcs[i]->arg_types[j]))
                    compat = false;
            if (!compat) continue;
            func = ir->funcs[i];
            break;
        }
    if (!func) {
        free(args);
        return ir_set_error(ir, "Unable to find compatible overload of \"%s\".", call->func), NULL;
    }
    
    size_t new_func_count = func_count + 1;
    char** new_funcs = alloc_mem(sizeof(char*)*new_func_count);
    for (size_t i = 0; i < func_count; i++)
        new_funcs[i] = funcs[i];
    new_funcs[func_count] = func->name;
    
    for (size_t i = 0; i < func->arg_count; i++) {
        ir_var_decl_t* dest = decl_var(ir, func->arg_names[i], get_dtype_comp(func->arg_types[i]), new_func_count, new_funcs);
        for (size_t j = 0; j < 4; j++) dest->current_ver[j]++;
        ir_inst_t inst;
        inst.op = IR_OP_MOV;
        inst.operand_count = 2;
        for (size_t j = 0; j < dest->comp; j++) {
            inst.operands[0] = create_var_operand(get_var_comp(dest, j));
            inst.operands[1] = create_var_operand(get_var_comp(args[i], j));
            add_inst(ir, &inst);
        }
    }
    free(args);
    
    for (size_t i = 0; i < func->stmt_count; i++) {
        bool ret = false;
        ir_var_decl_t* res = node_to_ir(func->stmts[i], ir, &ret, new_func_count, new_funcs);
        if (!res) {
            free(new_funcs);
            return NULL;
        }
        
        if (ret) {
            free(new_funcs);
            return res;
        }
    }
    
    return gen_temp_var(ir, get_dtype_comp(func->ret_type));
}

static ir_var_decl_t* node_to_ir(node_t* node, ir_t* ir, bool* returned, size_t func_count, char** funcs) {
    switch (node->type) {
    case NODET_NUM: {
        ir_var_decl_t* dest = gen_temp_var(ir, 1);
        ir_inst_t inst;
        inst.op = IR_OP_MOV;
        inst.operand_count = 2;
        inst.operands[0] = create_var_operand(get_var_comp(dest, 0));
        inst.operands[1] = create_num_operand(((num_node_t*)node)->val);
        add_inst(ir, &inst);
        return dest;
    }
    case NODET_ID: {
        id_node_t* id = (id_node_t*)node;
        if (!strcmp(id->name, "true")) {
            const uint64_t truei = 0xFFFFFFFFFFFFFFFF;
            const double truef = *(const double*)&truei;
            ir_inst_t inst;
            inst.op = IR_OP_MOV;
            inst.operand_count = 2;
            ir_var_decl_t* dest = gen_temp_var(ir, 1);
            inst.operands[0] = create_var_operand(get_var_comp(dest, 0));
            inst.operands[1] = create_num_operand(truef);
            add_inst(ir, &inst);
            return dest;
        } else if (!strcmp(id->name, "false")) {
            const uint64_t falsei = 0;
            const double falsef = *(const double*)&falsei;
            ir_inst_t inst;
            inst.op = IR_OP_MOV;
            inst.operand_count = 2;
            ir_var_decl_t* dest = gen_temp_var(ir, 1);
            inst.operands[0] = create_var_operand(get_var_comp(dest, 0));
            inst.operands[1] = create_num_operand(falsef);
            add_inst(ir, &inst);
            return dest;
        } else {
            ir_var_decl_t* var = get_id_var_decl(ir, id, func_count, funcs);
            if (!var)
                return ir_set_error(ir, "Unknown variable \"%s\".", id->name), NULL;
            return var;
        }
    }
    case NODET_ASSIGN: {
        bin_node_t* bin = (bin_node_t*)node;
        
        ir_var_decl_t* dest_var;
        if (bin->lhs->type == NODET_VAR_DECL || bin->lhs->type == NODET_PROP_DECL) {
            dest_var = node_to_ir(bin->lhs, ir, returned, func_count, funcs);
            if (!dest_var) return NULL;
        } else {
            node_t* dest = bin->lhs;
            while (dest->type == NODET_MEMBER) dest = ((bin_node_t*)dest)->lhs;
            dest_var = get_id_var_decl(ir, (id_node_t*)dest, func_count, funcs);
            if (!dest_var)
                return ir_set_error(ir, "Unknown variable \"%s\".", ((id_node_t*)dest)->name), NULL;
        }
        
        size_t dest_swizzle[4] = {0, 1, 2, 3};
        size_t dest_comp = dest_var->comp;
        node_t* lhs = bin->lhs;
        while (lhs->type == NODET_MEMBER) {
            char* swizzle = ((id_node_t*)((bin_node_t*)lhs)->rhs)->name;
            if (strlen(swizzle) > dest_comp)
                return ir_set_error(ir, "Invalid swizzle."), NULL;
            dest_comp = strlen(swizzle);
            for (size_t i = 0; i < dest_comp; i++)
                dest_swizzle[i] = get_comp_from_c(swizzle[i]);
            lhs = ((bin_node_t*)lhs)->lhs;
        }
        
        ir_var_decl_t* src_var = node_to_ir(bin->rhs, ir, returned, func_count, funcs);
        if (!src_var) return NULL;
        
        if (src_var->comp != dest_comp)
            return ir_set_error(ir, "Assignment of incompatible types."), NULL;
        
        ir_inst_t inst;
        inst.op = IR_OP_MOV;
        inst.operand_count = 2;
        for (size_t i = 0; i < src_var->comp; i++) {
            inst.operands[0] = create_var_operand(get_var_comp(dest_var, dest_swizzle[i]));
            inst.operands[0].var.ver++;
            inst.operands[1] = create_var_operand(get_var_comp(src_var, i));
            add_inst(ir, &inst);
            dest_var->current_ver[dest_swizzle[i]]++;
        }
        return dest_var;
    }
    case NODET_ADD:
    case NODET_SUB:
    case NODET_MUL:
    case NODET_DIV:
    case NODET_POW:
    case NODET_LESS:
    case NODET_GREATER:
    case NODET_EQUAL:
    case NODET_BOOL_AND:
    case NODET_BOOL_OR: {
        bin_node_t* bin = (bin_node_t*)node;
        
        ir_var_decl_t* lhs = node_to_ir(bin->lhs, ir, returned, func_count, funcs);
        ir_var_decl_t* rhs = node_to_ir(bin->rhs, ir, returned, func_count, funcs);
        if (!lhs || !rhs) return NULL;
        
        if (lhs->comp != rhs->comp)
            return ir_set_error(ir, "Incompatible types in binary operation."), NULL;
        
        ir_var_decl_t* dest = gen_temp_var(ir, lhs->comp);
        
        ir_inst_t inst;
        switch (node->type) {
        case NODET_ADD: inst.op = IR_OP_ADD; break;
        case NODET_SUB: inst.op = IR_OP_SUB; break;
        case NODET_MUL: inst.op = IR_OP_MUL; break;
        case NODET_DIV: inst.op = IR_OP_DIV; break;
        case NODET_POW: inst.op = IR_OP_POW; break;
        case NODET_LESS: inst.op = IR_OP_LESS; break;
        case NODET_GREATER: inst.op = IR_OP_GREATER; break;
        case NODET_EQUAL: inst.op = IR_OP_EQUAL; break;
        case NODET_BOOL_AND: inst.op = IR_OP_BOOL_AND; break;
        case NODET_BOOL_OR: inst.op = IR_OP_BOOL_OR; break;
        default: assert(false);
        }
        inst.operand_count = 3;
        for (size_t i = 0; i < lhs->comp; i++) {
            inst.operands[0] = create_var_operand(get_var_comp(dest, i));
            inst.operands[1] = create_var_operand(get_var_comp(lhs, i));
            inst.operands[2] = create_var_operand(get_var_comp(rhs, i));
            add_inst(ir, &inst);
        }
        return dest;
    }
    case NODET_MEMBER: {
        bin_node_t* bin = (bin_node_t*)node;
        
        if (bin->rhs->type != NODET_ID)
            return ir_set_error(ir, "Invalid swizzle."), NULL;
        
        ir_var_decl_t* src = node_to_ir(bin->lhs, ir, returned, func_count, funcs);
        if (!src) return NULL;
        char* swizzle = ((id_node_t*)bin->rhs)->name;
        
        if (strlen(swizzle)<1 || strlen(swizzle)>4)
            return ir_set_error(ir, "Invalid swizzle."), NULL;
        
        ir_var_decl_t* dest = gen_temp_var(ir, strlen(swizzle));
        
        ir_inst_t inst;
        inst.op = IR_OP_MOV;
        inst.operand_count = 2;
        for (size_t i = 0; i < dest->comp; i++) {
            char mem = swizzle[i];
            if (!is_valid_comp(mem, src->comp))
                return ir_set_error(ir, "Invalid swizzle."), NULL;
            inst.operands[0] = create_var_operand(get_var_comp(dest, i));
            inst.operands[1] = create_var_operand(get_var_comp(src, get_comp_from_c(mem)));
            add_inst(ir, &inst);
        }
        return dest;
    }
    case NODET_VAR_DECL:
    case NODET_PROP_DECL:
    case NODET_UNI_DECL: {
        decl_node_t* decl = (decl_node_t*)node;
        ir_var_decl_t* var = decl_var(ir, decl->name, get_dtype_comp(decl->dtype), func_count, funcs);
        if (node->type == NODET_PROP_DECL)
            ir->props = append_mem(ir->props, ir->prop_count++, sizeof(ir_var_decl_t*), &var);
        else if (node->type == NODET_UNI_DECL)
            ir->unis = append_mem(ir->unis, ir->uni_count++, sizeof(ir_var_decl_t*), &var);
        return var;
    }
    case NODET_RETURN: {
        *returned = true;
        return node_to_ir(((unary_node_t*)node)->val, ir, returned, func_count, funcs);
    }
    case NODET_CALL: {
        call_node_t* call = (call_node_t*)node;
        return gen_func_call(ir, call, func_count, funcs);
    }
    case NODET_FUNC_DECL: {
        ir->funcs = append_mem(ir->funcs, ir->func_count++, sizeof(node_t*), &node);
        return gen_temp_var(ir, 0);
    }
    case NODET_NEG:
    case NODET_BOOL_NOT: {
        unary_node_t* unary = (unary_node_t*)node;
        
        ir_var_decl_t* src = node_to_ir(unary->val, ir, returned, func_count, funcs);
        if (!src) return NULL;
        ir_var_decl_t* dest = gen_temp_var(ir, src->comp);
        
        ir_inst_t inst;
        inst.op = node->type==NODET_NEG ? IR_OP_NEG : IR_OP_BOOL_NOT;
        for (size_t i = 0; i < src->comp; i++) {
            inst.operand_count = 2;
            inst.operands[0] = create_var_operand(get_var_comp(dest, i));
            inst.operands[1] = create_var_operand(get_var_comp(src, i));
            add_inst(ir, &inst);
        }
        return dest;
    }
    case NODET_DROP: {
        return gen_temp_var(ir, 0);
    }
    case NODET_NOP: {return gen_temp_var(ir, 0);}
    case NODET_IF: {
        if_node_t* if_ = (if_node_t*)node;
        
        ir_var_decl_t* cond = node_to_ir(if_->condition, ir, returned, func_count, funcs);
        if (cond->comp != 1)
            return ir_set_error(ir, "Result type of condition must a boolean"), NULL;
        
        ir_inst_t inst;
        inst.op = IR_OP_BEGIN_IF;
        inst.operand_count = 1;
        inst.operands[0] = create_var_operand(get_var_comp(cond, 0));
        size_t begin = add_inst(ir, &inst)->id;
        
        size_t last_var_count = ir->var_count;
        unsigned int* last_vers = alloc_mem(sizeof(unsigned int)*ir->var_count*4);
        for (size_t i = 0; i < ir->var_count; i++)
            memcpy(last_vers+i*4, ir->vars[i]->current_ver, 4*sizeof(unsigned int));
        
        for (size_t i = 0; i < if_->stmt_count; i++) {
            if (!node_to_ir(if_->stmts[i], ir, returned, func_count, funcs)) {
                free(last_vers);
                return NULL;
            }
            if (*returned) break;
        }
        
        inst.op = IR_OP_END_IF;
        inst.operand_count = 0;
        inst.begin_if = begin;
        size_t end = add_inst(ir, &inst)->id;
        ir->insts[begin].end_if = end;
        
        //This assumes new variables are added to the end of the list
        //and that it is never reordered
        for (size_t i = 0; i < last_var_count; i++)
            for (size_t j = 0; j < 4; j++)
                if (last_vers[i*4+j] != ir->vars[i]->current_ver[j]) {
                    inst.op = IR_OP_PHI;
                    inst.operand_count = 3;
                    inst.operands[0] = create_var_operand(get_var_comp(ir->vars[i], j));
                    inst.operands[1] = inst.operands[0];
                    inst.operands[2] = inst.operands[0];
                    inst.operands[2].var.ver = last_vers[i*4+j];
                    ir->vars[i]->current_ver[j]++;
                    inst.operands[0].var.ver++;
                    inst.end_if = end;
                    add_inst(ir, &inst);
                }
        free(last_vers);
        
        return gen_temp_var(ir, 0);
    }
    }
    
    assert(false);
}

void get_vars(ir_inst_t* insts, size_t inst_count, size_t* var_count, ir_var_t** vars) {
    for (size_t i = 0; i < inst_count; i++) {
        ir_inst_t* inst = &insts[i];
        for (size_t j = 0; j < inst->operand_count; j++) {
            if (inst->operands[j].type != IR_OPERAND_VAR) continue;
            ir_var_t var = inst->operands[j].var;
            bool found = false;
            for (size_t k = 0; k < *var_count; k++) {
                ir_var_t* var2 = *vars + k;
                if (var2->decl==var.decl && var2->ver==var.ver &&
                    var2->comp_idx==var.comp_idx) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                *vars = append_mem(*vars, *var_count, sizeof(ir_var_t), &var);
                (*var_count)++;
            }
        }
    }
}

bool create_ir(const ast_t* ast, ir_t* ir) {
    memset(ir, 0, sizeof(ir_t));
    
    bool returned = false;
    for (size_t i = 0; i<ast->stmt_count && !returned; i++) {
        if (!node_to_ir(ast->stmts[i], ir, &returned, 0, NULL)) {
            free_ir(ir);
            memset(ir, 0, sizeof(ir_t));
            return false;
        }
    }
    
    for (size_t i = 0; i < ir->prop_count; i++) {
        ir_var_decl_t* var = NULL;
        for (size_t j = 0; j < ir->var_count; j++)
            if (ir->vars[j]->name.func_count==0 &&
                !strcmp(ir->vars[j]->name.name, ir->props[i]->name.name)) {
                var = ir->vars[j];
                break;
            }
        assert(var);
        
        ir_inst_t inst;
        inst.op = IR_OP_STORE_PROP;
        inst.operand_count = 2;
        for (size_t j = 0; j < ir->props[i]->comp; j++) {
            inst.operands[0] = create_prop_operand(i, j);
            inst.operands[1] = create_var_operand(get_var_comp(var, j));
            add_inst(ir, &inst);
        }
    }
    
    return true;
}

void free_ir(ir_t* ir) {
    free(ir->insts);
    
    for (size_t i = 0; i < ir->var_count; i++) {
        ir_var_decl_t* var = ir->vars[i];
        for (size_t j = 0; j < var->name.func_count; j++) free(var->name.funcs[j]);
        free(var->name.funcs);
        free(var->name.name);
        free(ir->vars[i]);
    }
    free(ir->vars);
    free(ir->funcs);
    
    free(ir->props);
    free(ir->unis);
}

void remove_redundant_moves(ir_t* ir) {
    size_t replace_count = 0;
    ir_var_t* replace_keys = NULL;
    ir_var_t* replace_vals = NULL;
    
    ir_inst_t* insts = ir->insts;
    size_t inst_count = ir->inst_count;
    ir->insts = NULL;
    ir->inst_count = 0;
    for (size_t i = 0; i < inst_count; i++) {
        ir_inst_t inst = insts[i];
        
        if (inst.op==IR_OP_MOV && inst.operands[1].type == IR_OPERAND_VAR) {
            replace_keys = append_mem(replace_keys, replace_count, sizeof(ir_var_t), &inst.operands[0].var);
            
            ir_var_t var = inst.operands[1].var;
            
            int idx = -1;
            for (size_t j = 0; j < replace_count; j++)
                if (replace_keys[j].decl==var.decl && replace_keys[j].ver==var.ver &&
                    replace_keys[j].comp_idx==var.comp_idx) {
                    idx = j;
                    break;
                }
            
            if (idx != -1) var = replace_vals[idx];
            
            replace_vals = append_mem(replace_vals, replace_count, sizeof(ir_var_t), &var);
            replace_count++;
        } else {
            for (size_t j = 0; j < inst.operand_count; j++) {
                if (inst.operands[j].type != IR_OPERAND_VAR) continue;
                ir_var_t var = inst.operands[j].var;
                int idx = -1;
                for (size_t k = 0; k < replace_count; k++)
                    if (replace_keys[k].decl==var.decl && replace_keys[k].ver==var.ver &&
                        replace_keys[k].comp_idx==var.comp_idx) {
                        idx = k;
                        break;
                    }
                
                if (idx != -1)
                    inst.operands[j].var = replace_vals[idx];
            }
            add_inst(ir, &inst)->id = inst.id;
        }
    }
    free(insts);
    free(replace_keys);
    free(replace_vals);
}

void add_drop_insts(ir_t* ir) {
    size_t var_count = 0;
    ir_var_t* vars = NULL;
    size_t inst_count = ir->inst_count;
    ir_inst_t* insts = ir->insts;
    ir->inst_count = 0;
    ir->insts = NULL;
    for (size_t i = 0; i < inst_count; i++) {
        add_inst(ir, insts+i)->id = insts[i].id;
        
        get_vars(insts+i, 1, &var_count, &vars);
        
        size_t future_count = 0;
        ir_var_t* future = NULL;
        get_vars(insts+i+1, inst_count-i-1, &future_count, &future);
        
        for (ptrdiff_t j = 0; j < var_count; j++) {
            for (size_t k = 0; k < future_count; k++)
                if (vars[j].ver==future[k].ver && vars[j].decl==future[k].decl &&
                    vars[j].comp_idx==future[k].comp_idx) goto end;
            
            ir_inst_t inst;
            inst.op = IR_OP_DROP;
            inst.operand_count = 1;
            inst.operands[0] = create_var_operand(vars[j]);
            add_inst(ir, &inst);
            
            memmove(vars+j, vars+j+1, (var_count-j-1)*sizeof(ir_var_t));
            if (var_count-1)
                vars = realloc_mem(vars, (var_count-1)*sizeof(ir_var_t));
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
    free(insts);
    free(vars);
}
