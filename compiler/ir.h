#ifndef IR_H
#define IR_H
#include <stddef.h>
#include <stdbool.h>

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
    IR_OP_DROP,
    IR_OP_LESS,
    IR_OP_GREATER,
    IR_OP_EQUAL,
    IR_OP_BOOL_AND,
    IR_OP_BOOL_OR,
    IR_OP_BOOL_NOT,
    IR_OP_SQRT,
    IR_OP_STORE_ATTR, //Must be at the end
    IR_OP_SEL,
    IR_OP_IF,
    IR_OP_WHILE,
    IR_OP_PHI
} ir_opcode_t;

typedef enum {
    IR_OPERAND_NUM,
    IR_OPERAND_VAR,
    IR_OPERAND_ATTR //Only valid with IR_OP_LOAD_ATTR and IR_OP_STORE_ATTR
} ir_operand_type_t;

typedef struct ast_t ast_t;

typedef struct {
    size_t func_count;
    char** funcs;
    char* name;
} ir_var_name_t;

typedef struct {
    ir_var_name_t name;
    size_t comp;
    unsigned int current_ver[4];
} ir_var_decl_t;

typedef struct {
    ir_var_decl_t* decl;
    unsigned int ver;
    unsigned int comp_idx;
} ir_var_t;

typedef struct {
    unsigned int index;
    unsigned int comp;
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
    size_t id;
    ir_opcode_t op;
    size_t operand_count;
    ir_operand_t operands[IR_OPERAND_MAX];
    union {
        size_t phi_inst_cond;
        struct {
            size_t inst_count;
            ir_inst_t* insts;
        };
        struct {
            size_t cond_inst_count;
            ir_inst_t* cond_insts;
            size_t body_inst_count;
            ir_inst_t* body_insts;
        };
    };
};

typedef struct {
    size_t inst_count;
    ir_inst_t* insts;
    char error[1024];
    unsigned int next_temp_var;
    
    size_t var_count;
    ir_var_decl_t** vars;
    
    size_t func_count;
    func_decl_node_t** funcs;
    
    size_t attr_count;
    ir_var_decl_t** attrs;
    
    size_t uni_count;
    ir_var_decl_t** unis;
    
    size_t next_inst_id;
} ir_t;

bool create_ir(const ast_t* ast, ir_t* ir);
void free_ir(ir_t* ir);
void remove_redundant_moves(ir_t* ir);
void add_drop_insts(ir_t* ir);
#endif
