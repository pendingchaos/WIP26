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
    BC_OP_ADD = 0,
    BC_OP_SUB = 1,
    BC_OP_MUL = 2,
    BC_OP_DIV = 3,
    BC_OP_POW = 4,
    BC_OP_MOVF = 5,
    BC_OP_MIN = 6,
    BC_OP_MAX = 7,
    BC_OP_SQRT = 8,
    BC_OP_LOAD_PROP = 9,
    BC_OP_STORE_PROP = 10,
    BC_OP_DELETE = 11,
    BC_OP_LESS = 12,
    BC_OP_GREATER = 13,
    BC_OP_EQUAL = 14,
    BC_OP_BOOL_AND = 15,
    BC_OP_BOOL_OR = 16,
    BC_OP_BOOL_NOT = 17,
    BC_OP_SEL = 18,
    BC_OP_COND_BEGIN = 19,
    BC_OP_COND_END = 20,
    BC_OP_END = 21
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
    size_t bc_size;
    uint8_t* bc;
    
    size_t var_count;
    ir_var_t* vars;
    uint8_t* var_regs;
    
    ir_var_decl_t temp_var;
    uint8_t min_reg;
    uint8_t max_reg;
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

static int _get_reg(gen_bc_state_t* state, ir_var_t var) {
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

static int get_reg(gen_bc_state_t* state, ir_var_t var) {
    int reg = _get_reg(state, var);
    if (reg!=-1 && reg<state->min_reg) state->min_reg = reg;
    if (reg!=-1 && reg>state->max_reg) state->max_reg = reg;
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

static ir_var_t gen_tmp_var(gen_bc_state_t* state) {
    unsigned int ver = state->temp_var.current_ver[0]++;
    ir_var_t var;
    var.decl = &state->temp_var;
    var.ver = ver;
    var.comp_idx = 0;
    return var;
}

#define WRITEB(b_) do {uint8_t b = b_;state->bc = append_mem(state->bc, state->bc_size, 1, &b);state->bc_size++;} while (0);
#define WRITEF(f) do {state->bc = realloc_mem(state->bc, state->bc_size+4);*(float*)(state->bc+state->bc_size)=f;state->bc_size+=4;} while (0);
#define WRITEU32(i) do {state->bc = realloc_mem(state->bc, state->bc_size+4);*(uint32_t*)(state->bc+state->bc_size)=i;state->bc_size+=4;} while (0);

static bool write_bin(gen_bc_state_t* state, int dest_reg, const ir_inst_t* inst) {
    ir_var_t lhs, rhs;
    
    bool lhs_num = inst->operands[1].type == IR_OPERAND_NUM;
    bool rhs_num = inst->operands[2].type == IR_OPERAND_NUM;
    
    if (lhs_num) lhs = gen_tmp_var(state);
    else lhs = inst->operands[1].var;
    
    if (rhs_num) rhs = gen_tmp_var(state);
    else rhs = inst->operands[2].var;
    
    int lhs_reg = get_reg(state, lhs);
    int rhs_reg = get_reg(state, rhs);
    if (lhs_reg<0 || rhs_reg<0) return false;
    
    if (lhs_num) {
        WRITEB(BC_OP_MOVF);
        WRITEB(lhs_reg);
        WRITEF(inst->operands[1].num);
    }
    
    if (rhs_num) {
        WRITEB(BC_OP_MOVF);
        WRITEB(rhs_reg);
        WRITEF(inst->operands[2].num);
    }
    
    switch (inst->op) {
    case IR_OP_ADD: WRITEB(BC_OP_ADD); break;
    case IR_OP_SUB: WRITEB(BC_OP_SUB); break;
    case IR_OP_MUL: WRITEB(BC_OP_MUL); break;
    case IR_OP_DIV: WRITEB(BC_OP_DIV); break;
    case IR_OP_POW: WRITEB(BC_OP_POW); break;
    case IR_OP_MIN: WRITEB(BC_OP_MIN); break;
    case IR_OP_MAX: WRITEB(BC_OP_MAX); break;
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
    
    if (lhs_num) drop_var(state, lhs);
    if (rhs_num) drop_var(state, rhs);
    
    return true;
}

static bool write_mov(gen_bc_state_t* state, const ir_inst_t* inst) {
    int dest_reg = get_reg(state, inst->operands[0].var);
    if (dest_reg < 0) return false;
    
    if (inst->operands[1].type == IR_OPERAND_VAR) {
        ir_var_t lhs_var = gen_tmp_var(state);
        
        int lhs = get_reg(state, lhs_var);
        int rhs = get_reg(state, inst->operands[1].var);
        if (lhs<0 || rhs<0) return false;
        
        WRITEB(BC_OP_MOVF);
        WRITEB(lhs);
        WRITEF(0.0f);
        
        WRITEB(BC_OP_ADD);
        WRITEB(dest_reg)
        WRITEB(lhs);
        WRITEB(rhs);
        
        drop_var(state, lhs_var);
    } else {
        WRITEB(BC_OP_MOVF);
        WRITEB(dest_reg);
        WRITEF(inst->operands[1].num);
    }
    
    return true;
}

static bool write_unary(gen_bc_state_t* state, const ir_inst_t* inst) {
    ir_var_t lhs_var;
    bool num = inst->operands[1].type == IR_OPERAND_NUM;
    
    if (num) lhs_var = gen_tmp_var(state);
    else lhs_var = inst->operands[1].var;
    
    int dest_reg = get_reg(state, inst->operands[0].var);
    int lhs_reg = get_reg(state, lhs_var);
    
    if (dest_reg<0 || lhs_reg<0) return false;
    
    if (num) {
        WRITEB(BC_OP_MOVF);
        WRITEB(lhs_reg);
        WRITEF(inst->operands[1].num);
    }
    
    switch (inst->op) {
    case IR_OP_BOOL_NOT: WRITEB(BC_OP_BOOL_NOT); break;
    case IR_OP_SQRT: WRITEB(BC_OP_SQRT); break;
    default: assert(false); break;
    }
    
    WRITEB(dest_reg);
    WRITEB(lhs_reg);
    
    if (num) drop_var(state, lhs_var);
    
    return true;
}

static bool write_neg(gen_bc_state_t* state, const ir_inst_t* inst) {
    int dest_reg = get_reg(state, inst->operands[0].var);
    if (dest_reg < 0) return false;
    
    if (inst->operands[1].type == IR_OPERAND_VAR) {
        ir_var_t lhs = gen_tmp_var(state);
        
        int lhs_reg = get_reg(state, lhs);
        int rhs_reg = get_reg(state, inst->operands[1].var);
        if (rhs_reg < 0) return false;
        
        WRITEB(BC_OP_SUB);
        WRITEB(dest_reg);
        WRITEB(lhs_reg);
        WRITEB(rhs_reg);
        
        drop_var(state, lhs);
    } else {
        WRITEB(BC_OP_MOVF);
        WRITEB(dest_reg);
        WRITEF(-inst->operands[1].num);
    }
    
    return true;
}

static bool write_sel(gen_bc_state_t* state, const ir_inst_t* inst) {
    bool a_num = inst->operands[1].type == IR_OPERAND_NUM;
    bool b_num = inst->operands[2].type == IR_OPERAND_NUM;
    bool cond_num = inst->operands[3].type == IR_OPERAND_NUM;
    
    ir_var_t a_var, b_var, cond_var;
    if (a_num) a_var = gen_tmp_var(state);
    else a_var = inst->operands[1].var;
    if (b_num) b_var = gen_tmp_var(state);
    else b_var = inst->operands[2].var;
    if (cond_num) b_var = gen_tmp_var(state);
    else cond_var = inst->operands[3].var;
    
    int dest_reg = get_reg(state, inst->operands[0].var);
    int a_reg = get_reg(state, inst->operands[1].var);
    int b_reg = get_reg(state, inst->operands[2].var);
    int cond_reg = get_reg(state, inst->operands[3].var);
    
    if (a_num) {
        WRITEB(BC_OP_MOVF);
        WRITEB(a_reg);
        WRITEF(inst->operands[1].num);
    }
    
    if (b_num) {
        WRITEB(BC_OP_MOVF);
        WRITEB(b_reg);
        WRITEF(inst->operands[2].num);
    }
    
    if (cond_num) {
        WRITEB(BC_OP_MOVF);
        WRITEB(cond_reg);
        WRITEF(inst->operands[3].num);
    }
    
    WRITEB(BC_OP_SEL);
    WRITEB(dest_reg);
    WRITEB(a_reg);
    WRITEB(b_reg);
    WRITEB(cond_reg);
    
    if (a_num) drop_var(state, a_var);
    if (b_num) drop_var(state, b_var);
    if (cond_num) drop_var(state, cond_var);
    
    return true;
}

static bool gen_bc(gen_bc_state_t* state, const ir_inst_t* insts, size_t inst_count, uint8_t* prop_indices, size_t* end_id) {
    for (size_t i = 0; i < inst_count; i++) {
        const ir_inst_t* inst = insts + i;
        if (end_id && inst->id==*end_id) {
            *end_id = i;
            return true;
        }
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
            if (!write_bin(state, dest_reg, inst)) goto error;
            break;
        }
        case IR_OP_NEG: {
            if (!write_neg(state, inst)) goto error;
            break;
        }
        case IR_OP_BOOL_NOT:
        case IR_OP_SQRT: {
            if (!write_unary(state, inst)) goto error;
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
        case IR_OP_SEL: {
            if (!write_sel(state, inst)) goto error;
            break;
        }
        case IR_OP_BEGIN_IF: {
            int cond_reg = get_reg(state, inst->operands[0].var);
            if (cond_reg < 0) goto error;
            
            size_t end = inst->end_if;
            gen_bc_state_t inner_state;
            inner_state.bc = NULL;
            inner_state.bc_size = 0;
            inner_state.var_count = state->var_count;
            inner_state.vars = alloc_mem(state->var_count*sizeof(ir_var_t));
            memcpy(inner_state.vars, state->vars, state->var_count*sizeof(ir_var_t));
            inner_state.var_regs = alloc_mem(state->var_count);
            memcpy(inner_state.var_regs, state->var_regs, state->var_count);
            inner_state.temp_var = state->temp_var;
            inner_state.min_reg = 255;
            inner_state.max_reg = 0;
            if (!gen_bc(&inner_state, insts+i+1, inst_count-i-1, prop_indices, &end)) {
                free(inner_state.bc);
                free(inner_state.vars);
                free(inner_state.var_regs);
                return false;
            }
            
            free(inner_state.vars);
            free(inner_state.var_regs);
            
            WRITEB(BC_OP_COND_BEGIN);
            WRITEB(cond_reg);
            WRITEU32(inner_state.bc_size);
            if (inner_state.max_reg < inner_state.min_reg) {
                WRITEB(0)
                WRITEB(0)
            } else {
                WRITEB(inner_state.min_reg);
                WRITEB(inner_state.max_reg);
            }
            
            state->bc = realloc_mem(state->bc, state->bc_size+inner_state.bc_size);
            memcpy(state->bc+state->bc_size, inner_state.bc, inner_state.bc_size);
            state->bc_size += inner_state.bc_size;
            free(inner_state.bc);
            
            WRITEB(BC_OP_COND_END);
            
            i += end+1; //Skip instructions and endif
            
            break;
        }
        case IR_OP_END_IF: {
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
    
    uint8_t* prop_indices = alloc_mem(ir->prop_count*4);
    size_t idx = 0;
    for (size_t i = 0; i < ir->prop_count; i++)
        for (size_t j = 0; j < ir->prop_comp[i]; j++) {
            prop_indices[i*4+j] = idx;
            idx++;
        }
    
    gen_bc_state_t state;
    state.bc = NULL;
    state.bc_size = 0;
    state.var_count = 0;
    state.vars = NULL;
    state.var_regs = NULL;
    state.temp_var.name.func_count = 0;
    state.temp_var.name.funcs = NULL;
    state.temp_var.name.name = "~bctmp";
    state.temp_var.comp = 1;
    state.temp_var.current_ver[0] = 0;
    state.min_reg = 255;
    state.max_reg = 0;
    if (!gen_bc(&state, ir->insts, ir->inst_count, prop_indices, NULL)) return false;
    
    uint32_t bc_size32 = htole32(state.bc_size);
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
    
    fwrite(state.bc, state.bc_size, 1, dest);
    
    free(state.bc);
    
    return true;
}

char* bc_get_error() {
    return error;
}
