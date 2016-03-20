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

static bool bc_set_error(bc_t* bc, const char* format, ...) {
    va_list list;
    va_start(list, format);
    vsnprintf(bc->error, sizeof(bc->error), format, list);
    va_end(list);
    return false;
}

typedef struct {
    bc_t* res_bc;
    
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
    
    if (reg < 0) bc_set_error(state->res_bc, "Unable to allocate register");
    
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

static bool redef(gen_bc_state_t* state, ir_var_t dest, ir_var_t src) {
    int reg = get_reg(state, dest);
    if (reg < 0) return false;
    uint8_t reg8 = reg;
    state->vars = append_mem(state->vars, state->var_count, sizeof(ir_var_t), &src);
    state->var_regs = append_mem(state->var_regs, state->var_count, 1, &reg8);
    state->var_count++;
    return true;
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

static ir_var_t _operands[16];
static bool _num_operands[16];

static int begin_operand(gen_bc_state_t* state, size_t index, ir_operand_t operand) {
    if (operand.type == IR_OPERAND_NUM) {
        _num_operands[index] = true;
        _operands[index] = gen_tmp_var(state);
        int reg = get_reg(state, _operands[index]);
        if (reg >= 0) {
            WRITEB(BC_OP_MOVF);
            WRITEB(reg);
            WRITEF(operand.num);
        }
        return reg;
    } else {
        _num_operands[index] = false;
        _operands[index] = operand.var;
        return get_reg(state, _operands[index]);
    }
}

static void end_operand(gen_bc_state_t* state, size_t index) {
    if (_num_operands[index]) drop_var(state, _operands[index]);
}

static bool write_bin(gen_bc_state_t* state, int dest_reg, const ir_inst_t* inst) {
    int lhs_reg = begin_operand(state, 0, inst->operands[1]);
    int rhs_reg = begin_operand(state, 1, inst->operands[2]);
    if (lhs_reg<0 || rhs_reg<0) return false;
    
    switch (inst->op) {
    case IR_OP_ADD: WRITEB(BC_OP_ADD); break;
    case IR_OP_SUB: WRITEB(BC_OP_SUB); break;
    case IR_OP_MUL: WRITEB(BC_OP_MUL); break;
    case IR_OP_DIV: WRITEB(BC_OP_DIV); break;
    case IR_OP_POW: WRITEB(BC_OP_POW); break;
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
    
    end_operand(state, 0);
    end_operand(state, 1);
    
    return true;
}

static bool write_mov(gen_bc_state_t* state, const ir_inst_t* inst) {
    int dest_reg = get_reg(state, inst->operands[0].var);
    if (dest_reg < 0) return false;
    
    if (inst->operands[1].type == IR_OPERAND_VAR) {
        if (!redef(state, inst->operands[0].var, inst->operands[1].var))
            return false;
    } else {
        WRITEB(BC_OP_MOVF);
        WRITEB(dest_reg);
        WRITEF(inst->operands[1].num);
    }
    
    return true;
}

static bool write_unary(gen_bc_state_t* state, const ir_inst_t* inst) {
    int dest_reg = get_reg(state, inst->operands[0].var);
    int lhs_reg = begin_operand(state, 0, inst->operands[1]);
    
    if (dest_reg<0 || lhs_reg<0) return false;
    
    switch (inst->op) {
    case IR_OP_BOOL_NOT: WRITEB(BC_OP_BOOL_NOT); break;
    case IR_OP_SQRT: WRITEB(BC_OP_SQRT); break;
    default: assert(false); break;
    }
    
    WRITEB(dest_reg);
    WRITEB(lhs_reg);
    
    end_operand(state, 0);
    
    return true;
}

static bool write_neg(gen_bc_state_t* state, const ir_inst_t* inst) {
    int dest_reg = get_reg(state, inst->operands[0].var);
    if (dest_reg < 0) return false;
    
    if (inst->operands[1].type == IR_OPERAND_VAR) {
        ir_var_t lhs = gen_tmp_var(state);
        
        int lhs_reg = get_reg(state, lhs);
        int rhs_reg = get_reg(state, inst->operands[1].var);
        if (lhs_reg<0 || rhs_reg<0) return false;
        
        WRITEB(BC_OP_MOVF);
        WRITEB(lhs_reg);
        WRITEF(0.0f);
        
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
    int a_reg = begin_operand(state, 0, inst->operands[1]);
    int b_reg = begin_operand(state, 1, inst->operands[2]);
    int cond_reg = begin_operand(state, 2, inst->operands[3]);
    int dest_reg = get_reg(state, inst->operands[0].var);
    
    if (a_reg<0 || b_reg<0 || cond_reg<0 || dest_reg<0) return false;
    
    WRITEB(BC_OP_SEL);
    WRITEB(dest_reg);
    WRITEB(a_reg);
    WRITEB(b_reg);
    WRITEB(cond_reg);
    
    end_operand(state, 0);
    end_operand(state, 1);
    end_operand(state, 2);
    
    return true;
}

static const ir_inst_t* find_by_id(const ir_inst_t* insts, size_t inst_count, size_t id) {
    for (size_t i = 0; i < inst_count; i++)
        if (insts[i].id == id) return insts + i;
	return NULL;
}

static bool _gen_bc(gen_bc_state_t* state, const ir_inst_t* insts, size_t inst_count, size_t* end_id) {
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
        case IR_OP_EMIT: {
            WRITEB(BC_OP_EMIT);
            size_t count = 0;
            for (size_t i = 0; i < state->res_bc->ir->attr_count; i++)
                count += state->res_bc->ir->attrs[i]->comp;
            WRITEB(count);
            for (size_t i = 0; i < state->res_bc->ir->attr_count; i++)
                for (size_t j = 0; j < state->res_bc->ir->attrs[i]->comp; j++)
                    WRITEB(state->res_bc->attr_store_regs[i*4+j]);
            break;
        }
        case IR_OP_DROP: {
            drop_var(state, inst->operands[0].var);
            break;
        }
        case IR_OP_SEL: {
            if (!write_sel(state, inst)) goto error;
            break;
        }
        case IR_OP_BEGIN_IF: {
            const ir_inst_t* end_if = find_by_id(insts, inst_count, inst->end);
            for (size_t j = end_if-insts+1; j < inst_count; j++) {
                const ir_inst_t* phi = insts + j;
                if (phi->op == IR_OP_PHI) {
                    if (!redef(state, phi->operands[2].var, phi->operands[0].var)) //res -> false
                        return false;
                    if (!redef(state, phi->operands[2].var, phi->operands[1].var)) //true -> false
                        return false;
                } else if (phi->op == IR_OP_DROP) {
                } else break;
            }
            
            int cond_reg = get_reg(state, inst->operands[0].var);
            if (cond_reg < 0) goto error;
            
            size_t end = inst->end;
            gen_bc_state_t inner_state = *state;
            inner_state.bc = NULL;
            inner_state.bc_size = 0;
            inner_state.min_reg = 255;
            inner_state.max_reg = 0;
            if (!_gen_bc(&inner_state, insts+i+1, inst_count-i-1, &end)) {
                free(inner_state.bc);
                return false;
            }
            state->vars = inner_state.vars;
            state->var_regs = inner_state.var_regs;
            state->var_count = inner_state.var_count;
            state->temp_var = inner_state.temp_var;
            
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
        case IR_OP_BEGIN_WHILE: {
            const ir_inst_t* end_cond = find_by_id(insts, inst_count, inst->end_while_cond);
            const ir_inst_t* end = find_by_id(insts, inst_count, end_cond->end_while);
            
            for (size_t j = end-insts+1; j < inst_count; j++) {
                const ir_inst_t* phi = insts + j;
                if (phi->op == IR_OP_PHI) {
                    if (!redef(state, phi->operands[2].var, phi->operands[0].var)) //res -> false
                        return false;
                    if (!redef(state, phi->operands[2].var, phi->operands[1].var)) //true -> false
                        return false;
                } else if (phi->op == IR_OP_DROP) {
                } else break;
            }
            
            gen_bc_state_t cond_state = *state;
            cond_state.bc = NULL;
            cond_state.bc_size = 0;
            cond_state.min_reg = 255;
            cond_state.max_reg = 0;
            size_t end_id = end_cond->id;
            if (!_gen_bc(&cond_state, inst+1, inst_count-i-1, &end_id)) {
                free(cond_state.bc);
                return false;
            }
            state->vars = cond_state.vars;
            state->var_regs = cond_state.var_regs;
            state->var_count = cond_state.var_count;
            state->temp_var = cond_state.temp_var;
            
            gen_bc_state_t body_state = *state;
            body_state.bc = NULL;
            body_state.bc_size = 0;
            body_state.min_reg = 255;
            body_state.max_reg = 0;
            end_id = end->id;
            if (!_gen_bc(&body_state, end_cond+1, inst_count-i-1, &end_id)) {
                free(body_state.bc);
                return false;
            }
            state->vars = body_state.vars;
            state->var_regs = body_state.var_regs;
            state->var_count = body_state.var_count;
            state->temp_var = body_state.temp_var;
            
            int cond_reg = get_reg(state, end->operands[0].var);
            if (cond_reg < 0) goto error;
            
            WRITEB(BC_OP_WHILE_BEGIN);
            WRITEB(cond_reg);
            WRITEU32(cond_state.bc_size+1);
            if (cond_state.max_reg < cond_state.min_reg) {
                WRITEB(0)
                WRITEB(0)
            } else {
                WRITEB(cond_state.min_reg);
                WRITEB(cond_state.max_reg);
            }
            WRITEU32(body_state.bc_size);
            if (body_state.max_reg < body_state.min_reg) {
                WRITEB(0)
                WRITEB(0)
            } else {
                WRITEB(body_state.min_reg);
                WRITEB(body_state.max_reg);
            }
            
            state->bc = realloc_mem(state->bc, state->bc_size+cond_state.bc_size);
            memcpy(state->bc+state->bc_size, cond_state.bc, cond_state.bc_size);
            state->bc_size += cond_state.bc_size;
            free(cond_state.bc);
            
            WRITEB(BC_OP_WHILE_END_COND);
            
            state->bc = realloc_mem(state->bc, state->bc_size+body_state.bc_size);
            memcpy(state->bc+state->bc_size, body_state.bc, body_state.bc_size);
            state->bc_size += body_state.bc_size;
            free(body_state.bc);
            
            WRITEB(BC_OP_WHILE_END);
            
            i += (end-insts)+1; //Skip instructions and endwhile
            break;
        }
        case IR_OP_END_IF:
        case IR_OP_END_WHILE:
        case IR_OP_END_WHILE_COND:
        case IR_OP_PHI: {
			break;
		}
        case IR_OP_STORE_ATTR: {
            ir_attr_t attr = inst->operands[0].attr;
            int reg = get_reg(state, inst->operands[1].var);
            if (reg < 0) return false;
            state->res_bc->attr_store_regs[attr.index*4+attr.comp] = reg;
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

bool gen_bc(bc_t* bc) {
    bc->error[0] = 0;
    
    bc->ptype = bc->ir->ptype;
    
    bc->attr_load_regs = alloc_mem(bc->ir->attr_count*4);
    bc->attr_store_regs = alloc_mem(bc->ir->attr_count*4);
    bc->uni_regs = alloc_mem(bc->ir->uni_count*4);
    
    gen_bc_state_t state;
    state.bc = NULL;
    state.bc_size = 0;
    state.var_count = 0;
    state.vars = NULL;
    state.var_regs = NULL;
    state.temp_var.name.func_count = 0;
    state.temp_var.name.funcs = NULL;
    state.temp_var.name.name = "~bctmp";
    state.temp_var.name.call_id = 0;
    state.temp_var.comp = 1;
    state.temp_var.current_ver[0] = 0;
    state.min_reg = 255;
    state.max_reg = 0;
    state.res_bc = bc;
    
    for (size_t i = 0; i < bc->ir->uni_count; i++) {
        ir_var_t var;
        var.decl = bc->ir->unis[i];
        for (size_t j = 0; j < var.decl->comp; j++) {
            var.ver = 0;
            var.comp_idx = j;
            int reg = get_reg(&state, var);
            if (reg < 0) return false;
            bc->uni_regs[i*4+j] = reg;
        }
    }
    
    for (size_t i = 0; i < bc->ir->attr_count; i++) {
        ir_var_t var;
        var.decl = bc->ir->attrs[i];
        for (size_t j = 0; j < var.decl->comp; j++) {
            var.ver = 0;
            var.comp_idx = j;
            int reg = get_reg(&state, var);
            if (reg < 0) return false;
            bc->attr_load_regs[i*4+j] = reg;
        }
    }
    
    if (!_gen_bc(&state, bc->ir->insts, bc->ir->inst_count, NULL)) return false;
    
    bc->bc_size = state.bc_size;
    bc->bc = state.bc;
    
    return true;
}

bool write_bc(FILE* dest, const bc_t* bc) {
    if (bc->ptype == PROGT_SIM) fwrite("SIMv0.0 ", 8, 1, dest);
    else fwrite("EMTv0.0 ", 8, 1, dest);
    
    uint8_t attr_count = 0;
    for (size_t i = 0; i < bc->ir->attr_count; i++)
        attr_count += bc->ir->attrs[i]->comp;
    fwrite(&attr_count, 1, 1, dest);
    
    uint8_t uni_count = 0;
    for (size_t i = 0; i < bc->ir->uni_count; i++)
        uni_count += bc->ir->unis[i]->comp;
    fwrite(&uni_count, 1, 1, dest);
    
    uint32_t bc_size32 = htole32(bc->bc_size);
    fwrite(&bc_size32, 4, 1, dest);
    
    for (size_t i = 0; i < bc->ir->attr_count; i++) {
        for (size_t j = 0; j < bc->ir->attrs[i]->comp; j++) {
            uint8_t name_len = strlen(bc->ir->attrs[i]->name.name)+2;
            fwrite(&name_len, 1, 1, dest);
            fwrite(bc->ir->attrs[i]->name.name, name_len-2, 1, dest);
            fwrite(".", 1, 1, dest);
            fwrite("xyzw"+j, 1, 1, dest);
            if (bc->ptype == PROGT_SIM) {
                uint8_t lreg = bc->attr_load_regs[i*4+j];
                uint8_t sreg = bc->attr_store_regs[i*4+j];
                fwrite(&lreg, 1, 1, dest);
                fwrite(&sreg, 1, 1, dest);
            }
        }
    }
    
    for (size_t i = 0; i < bc->ir->uni_count; i++) {
        for (size_t j = 0; j < bc->ir->unis[i]->comp; j++) {
            uint8_t name_len = strlen(bc->ir->unis[i]->name.name)+2;
            fwrite(&name_len, 1, 1, dest);
            fwrite(bc->ir->unis[i]->name.name, name_len-2, 1, dest);
            fwrite(".", 1, 1, dest);
            fwrite("xyzw"+j, 1, 1, dest);
            uint8_t reg = bc->uni_regs[i*4+j];
            fwrite(&reg, 1, 1, dest);
        }
    }
    
    fwrite(bc->bc, bc->bc_size, 1, dest);
    
    return true;
}

void free_bc(bc_t* bc) {
    free(bc->bc);
    free(bc->uni_regs);
    free(bc->attr_load_regs);
    free(bc->attr_store_regs);
}
