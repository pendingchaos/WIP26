#ifndef IR_H
#define IR_H
#include "shared.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define IR_OPERAND_MAX 4

typedef struct func_decl_node_t func_decl_node_t;

typedef enum {
    IR_OP_MOV,
    IR_OP_ADD,
    IR_OP_SUB,
    IR_OP_MUL,
    IR_OP_DIV,
    IR_OP_POW,
    IR_OP_NEG,
    IR_OP_DELETE,
    IR_OP_EMIT,
    IR_OP_DROP,
    IR_OP_LESS,
    IR_OP_GREATER,
    IR_OP_EQUAL,
    IR_OP_BOOL_AND,
    IR_OP_BOOL_OR,
    IR_OP_BOOL_NOT,
    IR_OP_SQRT,
    IR_OP_STORE_ATTR, //Must be at the end for simulation programs
    IR_OP_SEL,
    IR_OP_BEGIN_IF,
    IR_OP_END_IF,
    IR_OP_BEGIN_WHILE,
    IR_OP_END_WHILE_COND,
    IR_OP_END_WHILE,
    IR_OP_PHI
} ir_opcode_t;

typedef enum {
    IR_OPERAND_NUM,
    IR_OPERAND_VAR,
    IR_OPERAND_ATTR //Only valid with IR_OP_LOAD_ATTR and IR_OP_STORE_ATTR
} ir_operand_type_t;

typedef struct ast_t ast_t;

typedef struct {
    unsigned int func_count;
    unsigned int call_id;
    char** funcs;
    char* name;
} ir_var_name_t;

typedef struct {
    ir_var_name_t name;
    uint8_t comp;
    unsigned int current_ver[4];
} ir_var_decl_t;

typedef struct {
    ir_var_decl_t* decl;
    unsigned int ver;
    uint8_t comp_idx;
} ir_var_t;

typedef struct {
    uint8_t index;
    uint8_t comp;
} ir_attr_t;

typedef struct {
    ir_operand_type_t type;
    union {
        double num;
        ir_var_t var;
        ir_attr_t attr;
    };
} ir_operand_t;

typedef struct ir_inst_t ir_inst_t;
struct ir_inst_t {
    unsigned int id;
    ir_opcode_t op:8;
    uint8_t operand_count;
    ir_operand_t operands[IR_OPERAND_MAX];
    union {
        unsigned int end; //Only with IR_OP_BEGIN_IF and IR_OP_PHI
        unsigned int begin_if; //Only with IR_OP_END_IF
        unsigned int end_while_cond; //Only with IR_OP_BEGIN_WHILE
        unsigned int end_while; //Only with IR_OP_END_WHILE_COND
    };
};

typedef struct {
    prog_type_t ptype;
    
    unsigned int inst_count;
    ir_inst_t* insts;
    char error[1024];
    unsigned int next_temp_var;
    
    unsigned int var_count;
    ir_var_decl_t** vars;
    
    unsigned int func_count;
    func_decl_node_t** funcs;
    
    unsigned int attr_count;
    ir_var_decl_t** attrs;
    
    unsigned int uni_count;
    ir_var_decl_t** unis;
    
    unsigned int next_inst_id;
    unsigned int next_call_id;
} ir_t;

bool create_ir(const ast_t* ast, prog_type_t ptype, ir_t* ir); //AST should be validated
void free_ir(ir_t* ir);
void remove_redundant_moves(ir_t* ir);
void add_drop_insts(ir_t* ir);
#endif
