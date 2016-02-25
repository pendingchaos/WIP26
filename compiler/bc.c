#include "bc.h"

#include "ir.h"
#include "shared.h"

#include <stdint.h>
#include <endian.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <math.h>

typedef enum {
    BC_OP_ADD_R_F = 0,
    BC_OP_ADD_RR = 1,
    BC_OP_SUB_R_F = 2,
    BC_OP_SUB_F_R = 3,
    BC_OP_SUB_RR = 4,
    BC_OP_MUL_R_F = 5,
    BC_OP_MUL_RR = 6,
    BC_OP_DIV_R_F = 7,
    BC_OP_DIV_F_R = 8,
    BC_OP_DIV_RR = 9,
    BC_OP_POW_R_F = 10,
    BC_OP_POW_F_R = 11,
    BC_OP_POW_RR = 12,
    BC_OP_MOV_F = 13,
    BC_OP_MIN_RR = 14,
    BC_OP_MIN_R_F = 15,
    BC_OP_MAX_RR = 16,
    BC_OP_MAX_R_F = 17,
    BC_OP_SQRT = 18,
    BC_OP_LOAD_PROP = 19,
    BC_OP_STORE_PROP = 20,
    BC_OP_DELETE = 21,
    BC_OP_LESS = 22,
    BC_OP_GREATER = 23,
    BC_OP_EQUAL = 24,
    BC_OP_BOOL_AND = 25,
    BC_OP_BOOL_OR = 26,
    BC_OP_BOOL_NOT = 27
} bc_op_t;

static char error[1024];

static bool bc_set_error(const char* format, ...) {
    va_list list;
    va_start(list, format);
    vsnprintf(error, sizeof(error), format, list);
    va_end(list);
    return false;
}

typedef struct {
    size_t* bc_size;
    uint8_t** bc;
    
    size_t var_count;
    ir_var_t* vars;
    uint8_t* var_regs;
} gen_bc_state_t;

static int find_reg(gen_bc_state_t* state) {
    int reg = -1;
    for (size_t i = 0; i < 256; i++) {
        bool found = false;
        for (size_t j = 0; j < state->var_count; j++)
            if (state->var_regs[j] == i) found = true;
        if (found) continue;
        reg = i;
        break;
    }
    
    if (reg < 0) bc_set_error("Unable to allocate register");
    return reg;
}

static int get_reg(gen_bc_state_t* state, ir_var_t var) {
    for (size_t i = 0; i < state->var_count; i++) {
        ir_var_t var2 = state->vars[i];
        if (var2.decl==var.decl && var2.ver==var.ver && var2.comp_idx==var.comp_idx)
            return state->var_regs[i];
    }
    
    int reg = find_reg(state);
    
    if (reg != -1) {
        uint8_t reg8 = reg;
        state->vars = append_mem(state->vars, state->var_count, sizeof(ir_var_t), &var);
        state->var_regs = append_mem(state->var_regs, state->var_count, 1, &reg8);
        state->var_count++;
    }
    
    return reg;
}

static void drop_var(gen_bc_state_t* state, ir_var_t var) {
    int idx = -1;
    for (size_t i = 0; i < state->var_count; i++) {
        ir_var_t var2 = state->vars[i];
        if (var2.decl==var.decl && var2.ver==var.ver && var2.comp_idx==var.comp_idx) {
            idx = i;
            break;
        }
    }
    
    if (idx < 0) return;
    
    memmove(state->vars+idx, state->vars+idx+1, (state->var_count-idx-1)*sizeof(ir_var_t));
    memmove(state->var_regs+idx, state->var_regs+idx+1, state->var_count-idx-1);
    state->var_count--;
    state->vars = realloc_mem(state->vars, state->var_count*sizeof(ir_var_t));
    state->var_regs = realloc_mem(state->var_regs, state->var_count);
}

#define WRITEB(b_) do {uint8_t b = b_;*(state->bc) = append_mem(*(state->bc), *(state->bc_size), 1, &b);(*(state->bc_size))++;} while (0);
#define WRITEF(f) do {*(state->bc) = realloc_mem(*(state->bc), *(state->bc_size)+4);*(float*)(*(state->bc)+*(state->bc_size))=f;(*(state->bc_size))+=4;} while (0);

static bool write_bin_vv(gen_bc_state_t* state, int dest_reg, ir_inst_t* inst) {
    int lhs_reg = get_reg(state, inst->operands[1].var);
    int rhs_reg = get_reg(state, inst->operands[2].var);
    if (lhs_reg < 0) return false;
    if (rhs_reg < 0) return false;
    
    switch (inst->op) {
    case IR_OP_ADD: WRITEB(BC_OP_ADD_RR); break;
    case IR_OP_SUB: WRITEB(BC_OP_SUB_RR); break;
    case IR_OP_MUL: WRITEB(BC_OP_MUL_RR); break;
    case IR_OP_DIV: WRITEB(BC_OP_DIV_RR); break;
    case IR_OP_POW: WRITEB(BC_OP_POW_RR); break;
    case IR_OP_MIN: WRITEB(BC_OP_MIN_RR); break;
    case IR_OP_MAX: WRITEB(BC_OP_MAX_RR); break;
    case IR_OP_LESS: WRITEB(BC_OP_LESS); break;
    case IR_OP_GREATER: WRITEB(BC_OP_GREATER); break;
    case IR_OP_EQUAL: WRITEB(BC_OP_EQUAL); break;
    case IR_OP_BOOL_AND: WRITEB(BC_OP_BOOL_AND); break;
    case IR_OP_BOOL_OR: WRITEB(BC_OP_BOOL_OR); break;
    default: assert(false);
    }
    WRITEB(dest_reg);
    WRITEB(lhs_reg);
    WRITEB(rhs_reg);
    
    return true;
}

static bool write_bin_nv(gen_bc_state_t* state, int dest_reg, ir_inst_t* inst) {
    int rhs_reg = get_reg(state, inst->operands[2].var);
    if (rhs_reg < 0) return false;
    
    bool swap = false;
    switch (inst->op) {
    case IR_OP_ADD: WRITEB(BC_OP_ADD_R_F); break;
    case IR_OP_MUL: WRITEB(BC_OP_MUL_R_F); break;
    case IR_OP_MIN: WRITEB(BC_OP_MIN_R_F); break;
    case IR_OP_MAX: WRITEB(BC_OP_MAX_R_F); break;
    case IR_OP_SUB: WRITEB(BC_OP_SUB_F_R); swap = true; break;
    case IR_OP_DIV: WRITEB(BC_OP_DIV_F_R); swap = true; break;
    case IR_OP_POW: WRITEB(BC_OP_POW_F_R); swap = true; break;
    case IR_OP_LESS: break; //TODO
    case IR_OP_GREATER: break; //TODO
    case IR_OP_EQUAL: break; //TODO
    case IR_OP_BOOL_AND: break; //TODO
    case IR_OP_BOOL_OR: break; //TODO
    default: assert(false);
    }
    
    WRITEB(dest_reg);
    if (swap) {
        WRITEF(inst->operands[1].num);
        WRITEB(rhs_reg);
    } else {
        WRITEB(rhs_reg);
        WRITEF(inst->operands[1].num);
    }
    
    return true;
}

static bool write_bin_vn(gen_bc_state_t* state, int dest_reg, ir_inst_t* inst) {
    int lhs_reg = get_reg(state, inst->operands[1].var);
    if (lhs_reg < 0) return false;
    
    switch (inst->op) {
    case IR_OP_ADD: WRITEB(BC_OP_ADD_R_F); break;
    case IR_OP_SUB: WRITEB(BC_OP_SUB_R_F); break;
    case IR_OP_MUL: WRITEB(BC_OP_MUL_R_F); break;
    case IR_OP_DIV: WRITEB(BC_OP_DIV_R_F); break;
    case IR_OP_POW: WRITEB(BC_OP_POW_R_F); break;
    case IR_OP_MIN: WRITEB(BC_OP_MIN_R_F); break;
    case IR_OP_MAX: WRITEB(BC_OP_MAX_R_F); break;
    case IR_OP_LESS: break; //TODO
    case IR_OP_GREATER: break; //TODO
    case IR_OP_EQUAL: break; //TODO
    case IR_OP_BOOL_AND: break; //TODO
    case IR_OP_BOOL_OR: break; //TODO
    default: assert(false);
    }
    WRITEB(dest_reg);
    WRITEB(lhs_reg);
    WRITEF(inst->operands[2].num);
    
    return true;
}

static bool write_bin_nn(gen_bc_state_t* state, int dest_reg, ir_inst_t* inst) {
    const uint32_t truei = 0xFFFFFFFF;
    const uint32_t falsei = 0;
    const float truef = *(const float*)&truei;
    const float falsef = *(const float*)&falsei;
    
    WRITEB(BC_OP_MOV_F);
    WRITEB(dest_reg);
    switch (inst->op) {
    case IR_OP_ADD:
        WRITEF(inst->operands[1].num + inst->operands[2].num);
        break;
    case IR_OP_SUB:
        WRITEF(inst->operands[1].num - inst->operands[2].num);
        break;
    case IR_OP_MUL:
        WRITEF(inst->operands[1].num * inst->operands[2].num);
        break;
    case IR_OP_DIV:
        WRITEF(inst->operands[1].num / inst->operands[2].num);
        break;
    case IR_OP_POW:
        WRITEF(pow(inst->operands[1].num, inst->operands[2].num));
        break;
    case IR_OP_MIN:
        WRITEF(fmin(inst->operands[1].num, inst->operands[2].num));
        break;
    case IR_OP_MAX:
        WRITEF(fmax(inst->operands[1].num, inst->operands[2].num));
        break;
    case IR_OP_LESS:
        WRITEF((inst->operands[1].num<inst->operands[2].num) ? truef : falsef);
        break;
    case IR_OP_GREATER:
        WRITEF((inst->operands[1].num>inst->operands[2].num) ? truef : falsef);
        break;
    case IR_OP_EQUAL:
        WRITEF((inst->operands[1].num==inst->operands[2].num) ? truef : falsef);
        break;
    case IR_OP_BOOL_AND:
        WRITEF((*(uint32_t*)&inst->operands[1].num&&*(uint32_t*)&inst->operands[2].num) ? truef : falsef);
        break;
    case IR_OP_BOOL_OR:
        WRITEF((*(uint32_t*)&inst->operands[1].num||*(uint32_t*)&inst->operands[2].num) ? truef : falsef);
        break;
    default:
        assert(false);
    }
    
    return true;
}

static bool write_mov(gen_bc_state_t* state, ir_inst_t* inst) {
    int dest_reg = get_reg(state, inst->operands[0].var);
    if (dest_reg < 0) return false;
    
    if (inst->operands[1].type == IR_OPERAND_VAR) {
        int reg = get_reg(state, inst->operands[1].var);
        if (reg < 0) return false;
        
        WRITEB(BC_OP_ADD_R_F);
        WRITEB(dest_reg);
        WRITEB(reg);
        WRITEF(0.0f);
    } else {
        WRITEB(BC_OP_MOV_F);
        WRITEB(dest_reg);
        WRITEF(inst->operands[1].num);
    }
    
    return true;
}

static bool write_neg(gen_bc_state_t* state, ir_inst_t* inst) {
    int dest_reg = get_reg(state, inst->operands[0].var);
    if (dest_reg < 0) return false;
    
    if (inst->operands[1].type == IR_OPERAND_VAR) {
        int reg = get_reg(state, inst->operands[1].var);
        if (reg < 0) return false;
        
        WRITEB(BC_OP_SUB_F_R);
        WRITEB(dest_reg);
        WRITEF(0.0f);
        WRITEB(reg);
    } else {
        WRITEB(BC_OP_MOV_F);
        WRITEB(dest_reg);
        WRITEF(-inst->operands[1].num);
    }
    
    return true;
}

static bool write_bool_not(gen_bc_state_t* state, ir_inst_t* inst) {
    const uint32_t truei = 0xFFFFFFFF;
    const uint32_t falsei = 0;
    const float truef = *(const float*)&truei;
    const float falsef = *(const float*)&falsei;
    
    int dest_reg = get_reg(state, inst->operands[0].var);
    if (dest_reg < 0) return false;
    
    if (inst->operands[1].type == IR_OPERAND_VAR) {
        int reg = get_reg(state, inst->operands[1].var);
        if (reg < 0) return false;
        
        WRITEB(BC_OP_BOOL_NOT);
        WRITEB(dest_reg);
        WRITEF(0.0f);
        WRITEB(reg);
    } else {
        WRITEB(BC_OP_MOV_F);
        WRITEB(dest_reg);
        WRITEF(*(uint32_t*)&inst->operands[1].num ? falsef : truef);
    }
    
    return true;
}

static bool write_sqrt(gen_bc_state_t* state, ir_inst_t* inst) {
    int dest_reg = get_reg(state, inst->operands[0].var);
    if (dest_reg < 0) return false;
    
    if (inst->operands[1].type == IR_OPERAND_VAR) {
        int reg = get_reg(state, inst->operands[1].var);
        if (reg < 0) return false;
        
        WRITEB(BC_OP_SQRT);
        WRITEB(dest_reg);
        WRITEB(reg);
    } else {
        WRITEB(BC_OP_MOV_F);
        WRITEB(dest_reg);
        WRITEF(sqrt(inst->operands[1].num));
    }
    
    return true;
}

static bool gen_bc(const ir_t* ir, uint8_t** bc, size_t* bc_size, uint8_t* prop_indices) {
    gen_bc_state_t state_;
    state_.bc = bc;
    state_.bc_size = bc_size;
    state_.var_count = 0;
    state_.vars = NULL;
    state_.var_regs = NULL;
    gen_bc_state_t* state = &state_;
    
    for (size_t i = 0; i < ir->inst_count; i++) {
        ir_inst_t* inst = ir->insts + i;
        switch (inst->op) {
        case IR_OP_MOV: {
            if (!write_mov(state, inst)) goto error;
            break;
        }
        case IR_OP_ADD:
        case IR_OP_SUB:
        case IR_OP_MUL:
        case IR_OP_DIV:
        case IR_OP_POW:
        case IR_OP_MIN:
        case IR_OP_MAX:
        case IR_OP_LESS:
        case IR_OP_GREATER:
        case IR_OP_EQUAL:
        case IR_OP_BOOL_AND:
        case IR_OP_BOOL_OR: {
            int dest_reg = get_reg(state, inst->operands[0].var);
            if (dest_reg < 0) goto error;
            if (inst->operands[1].type==IR_OPERAND_VAR && inst->operands[2].type==IR_OPERAND_VAR) {
                if (!write_bin_vv(state, dest_reg, inst)) goto error;
            } else if (inst->operands[1].type==IR_OPERAND_NUM && inst->operands[2].type==IR_OPERAND_VAR) {
                if (!write_bin_nv(state, dest_reg, inst)) goto error;
            } else if (inst->operands[1].type==IR_OPERAND_VAR && inst->operands[2].type==IR_OPERAND_NUM) {
                if (!write_bin_vn(state, dest_reg, inst)) goto error;
            } else {
                if (!write_bin_nn(state, dest_reg, inst)) goto error;
            }
            break;
        }
        case IR_OP_NEG: {
            if (!write_neg(state, inst)) goto error;
            break;
        }
        case IR_OP_BOOL_NOT: {
            if (!write_bool_not(state, inst)) goto error;
            break;
        }
        case IR_OP_SQRT: {
            if (!write_sqrt(state, inst)) goto error;
            break;
        }
        case IR_OP_DELETE: {
            WRITEB(BC_OP_DELETE);
            break;
        }
        case IR_OP_DROP: {
            drop_var(state, inst->operands[0].var);
            break;
        }
        case IR_OP_LOAD_PROP: {
            int dest_reg = get_reg(state, inst->operands[0].var);
            if (dest_reg < 0) goto error;
            
            WRITEB(BC_OP_LOAD_PROP);
            WRITEB(dest_reg);
            ir_prop_t prop = inst->operands[1].prop;
            WRITEB(prop_indices[prop.index*4+prop.comp]);
            break;
        }
        case IR_OP_STORE_PROP: {
            int src_reg = get_reg(state, inst->operands[1].var);
            if (src_reg < 0) goto error;
            
            WRITEB(BC_OP_STORE_PROP);
            ir_prop_t prop = inst->operands[0].prop;
            WRITEB(prop_indices[prop.index*4+prop.comp]);
            WRITEB(src_reg);
            break;
        }
        }
    }
    
    free(state->vars);
    free(state->var_regs);
    return true;
    error:
        free(state->vars);
        free(state->var_regs);
        return false;
}

bool write_bc(FILE* dest, const ir_t* ir, bool simulation) {
    error[0] = 0;
    
    if (simulation)
        fwrite("SIMv0.0 ", 8, 1, dest);
    else
        fwrite("EMTv0.0 ", 8, 1, dest);
    
    uint8_t prop_count = 0;
    for (size_t i = 0; i < ir->prop_count; i++)
        prop_count += ir->prop_comp[i];
    fwrite(&prop_count, 1, 1, dest);
    
    size_t bc_size = 0;
    uint8_t* bc = NULL;
    uint8_t* prop_indices = alloc_mem(ir->prop_count*4);
    size_t idx = 0;
    for (size_t i = 0; i < ir->prop_count; i++)
        for (size_t j = 0; j < ir->prop_comp[i]; j++) {
            prop_indices[i*4+j] = idx;
            idx++;
        }
    
    if (!gen_bc(ir, &bc, &bc_size, prop_indices)) return false;
    
    uint32_t bc_size32 = htole32(bc_size);
    fwrite(&bc_size32, 4, 1, dest);
    
    for (size_t i = 0; i < ir->prop_count; i++) {
        for (size_t j = 0; j < ir->prop_comp[i]; j++) {
            uint8_t name_len = strlen(ir->properties[i])+2;
            fwrite(&name_len, 1, 1, dest);
            fwrite(ir->properties[i], name_len-2, 1, dest);
            fwrite(".", 1, 1, dest);
            fwrite("xyzw"+j, 1, 1, dest);
            fwrite(prop_indices+i*4+j, 1, 1, dest);
        }
    }
    free(prop_indices);
    
    fwrite(bc, bc_size, 1, dest);
    
    free(bc);
    
    return true;
}

char* bc_get_error() {
    return error;
}
