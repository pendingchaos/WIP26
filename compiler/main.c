#include "parser.h"
#include "ir.h"
#include "bc.h"
#include "shared.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <assert.h>

static const struct option options[] = {
    {"input", required_argument, NULL, 'i'},
    {"output", required_argument, NULL, 'o'},
    {"type", required_argument, NULL, 't'},
    {"incdir", required_argument, NULL, 'I'},
    {"debug", no_argument, NULL, 'd'}
};

static void print_node(node_t* node, unsigned int indent) {
    for (unsigned int i = 0; i < indent; i++) printf("    ");
    
    switch (node->type) {
    case NODET_NUM:
        printf("NUM: ");
        goto num;
    case NODET_ID:
        printf("ID: ");
        goto id;
    case NODET_ASSIGN:
        printf("ASSIGN: ");
        goto binary;
    case NODET_ADD:
        printf("ADD: ");
        goto binary;
    case NODET_SUB:
        printf("SUB: ");
        goto binary;
    case NODET_MUL:
        printf("MUL: ");
        goto binary;
    case NODET_DIV:
        printf("DIV: ");
        goto binary;
    case NODET_POW:
        printf("POW: ");
        goto binary;
    case NODET_LESS:
        printf("LESS: ");
        goto binary;
    case NODET_GREATER:
        printf("GREATER: ");
        goto binary;
    case NODET_EQUAL:
        printf("EQUAL: ");
        goto binary;
    case NODET_BOOL_AND:
        printf("BOOL_AND: ");
        goto binary;
    case NODET_BOOL_OR:
        printf("BOOL_OR: ");
        goto binary;
    case NODET_MEMBER:
        printf("MEMBER: ");
        goto binary;
    case NODET_VAR_DECL:
        printf("VAR_DECL: ");
        goto decl;
    case NODET_ATTR_DECL:
        printf("ATTR_DECL: ");
        goto decl;
    case NODET_UNI_DECL:
        printf("UNI_DECL: ");
        goto decl;
    case NODET_RETURN:
        printf("RETURN: ");
        goto unary;
    case NODET_CALL:
        printf("CALL: ");
        goto call;
    case NODET_FUNC_DECL:
        printf("FUNC_DECL: ");
        goto func_decl;
    case NODET_NOP:
        printf("NOP\n");
        return;
    case NODET_NEG:
        printf("NEG: ");
        goto unary;
    case NODET_BOOL_NOT:
        printf("BOOL_NOT: ");
        goto unary;
    case NODET_DROP:
        printf("DROP: ");
        goto unary;
    case NODET_IF:
        printf("IF: ");
        goto cond;
    case NODET_WHILE:
        printf("WHILE: ");
        goto cond;
    }
    
    num:
        printf("%f\n", ((num_node_t*)node)->val);
        return;
    id:
        printf("%s\n", ((id_node_t*)node)->name);
        return;
    binary:
        printf("\n");
        print_node(((bin_node_t*)node)->lhs, indent+1);
        print_node(((bin_node_t*)node)->rhs, indent+1);
        return;
    decl:
        printf("%s:%s\n", ((decl_node_t*)node)->name, ((decl_node_t*)node)->dtype);
        return;
    call:
        printf("%s\n", ((call_node_t*)node)->func);
        for (size_t i = 0; i < ((call_node_t*)node)->arg_count; i++)
            print_node(((call_node_t*)node)->args[i], indent+1);
        return;
    func_decl: {
        func_decl_node_t* decl = (func_decl_node_t*)node;
        printf("%s(", decl->name);
        for (size_t i = 0; i < decl->arg_count; i++) {
            printf("%s:%s", decl->arg_names[i], decl->arg_types[i]);
            if (i != decl->arg_count-1) printf(", ");
        }
        printf("):%s\n", decl->ret_type);
        
        for (size_t i = 0; i < decl->stmt_count; i++)
            print_node(decl->stmts[i], indent+1);
        
        return;
    }
    unary:
        printf("\n");
        print_node(((unary_node_t*)node)->val, indent+1);
        return;
    cond:
        printf("\n");
        cond_node_t* if_ = (cond_node_t*)node;
        print_node(if_->condition, indent+1);
        for (size_t i = 0; i < if_->stmt_count; i++)
            print_node(if_->stmts[i], indent+1);
        return;
}

static void print_inst(ir_t* ir, ir_inst_t inst, size_t indent) {
    //if (inst.op == IR_OP_DROP) return;
    
    printf("%zu\t", inst.id);
    
    for (size_t i = 0; i < indent; i++) printf("    ");
    
    switch (inst.op) {
    case IR_OP_MOV: printf("mov "); break;
    case IR_OP_ADD: printf("add "); break;
    case IR_OP_SUB: printf("sub "); break;
    case IR_OP_MUL: printf("mul "); break;
    case IR_OP_DIV: printf("div "); break;
    case IR_OP_POW: printf("pow "); break;
    case IR_OP_NEG: printf("neg "); break;
    case IR_OP_DELETE: printf("delete "); break;
    case IR_OP_LESS: printf("less "); break;
    case IR_OP_GREATER: printf("greater "); break;
    case IR_OP_EQUAL: printf("equal "); break;
    case IR_OP_BOOL_AND: printf("booland "); break;
    case IR_OP_BOOL_OR: printf("boolor "); break;
    case IR_OP_BOOL_NOT: printf("boolnot "); break;
    case IR_OP_SQRT: printf("sqrt "); break;
    case IR_OP_DROP: printf("drop "); break;
    case IR_OP_SEL: printf("sel "); break;
    case IR_OP_IF: printf("if "); break;
    case IR_OP_WHILE: printf("while "); break;
    case IR_OP_PHI: printf("phi "); break;
    case IR_OP_STORE_ATTR: printf("storep "); break;
    }
    
    for (size_t i = 0; i < inst.operand_count; i++) {
        ir_operand_t op = inst.operands[i];
        switch (op.type) {
        case IR_OPERAND_NUM:
            printf("%f ", op.num);
            break;
        case IR_OPERAND_VAR:
            for (size_t j = 0; j < op.var.decl->name.func_count; j++)
                printf("%s::", op.var.decl->name.funcs[j]);
            printf("%s_%u", op.var.decl->name.name, op.var.ver);
            switch (op.var.comp_idx) {
            case 0: printf(".x "); break;
            case 1: printf(".y "); break;
            case 2: printf(".z "); break;
            case 3: printf(".w "); break;
            }
            break;
        case IR_OPERAND_ATTR:
            printf("$%s.%c ", ir->attrs[op.attr.index]->name.name, "xyzw"[op.attr.comp]);
            break;
        }
    }
    
    if (inst.op == IR_OP_PHI)
        printf("cond instruction is %zu", inst.phi_inst_cond);
    putchar('\n');
    
    if (inst.op == IR_OP_IF)
        for (size_t i = 0; i < inst.inst_count; i++)
            print_inst(ir, inst.insts[i], indent+1);
    else if (inst.op == IR_OP_WHILE) {
        for (size_t i = 0; i < inst.cond_inst_count; i++)
            print_inst(ir, inst.cond_insts[i], indent+1);
        
        printf("\t");
        for (size_t i = 0; i <= indent; i++) printf("    ");
        printf("--------\n");
        
        for (size_t i = 0; i < inst.body_inst_count; i++)
            print_inst(ir, inst.body_insts[i], indent+1);
    }
}

static void print_bc(uint8_t* begin, uint8_t* end) {
    uint8_t* bc = begin;
    while (bc < end) {
        printf("%zu ", bc-begin);
        uint8_t op = *bc++;
        switch (op) {
        case BC_OP_ADD:
        case BC_OP_SUB:
        case BC_OP_MUL:
        case BC_OP_DIV:
        case BC_OP_POW:
        case BC_OP_LESS:
        case BC_OP_GREATER:
        case BC_OP_EQUAL:
        case BC_OP_BOOL_AND:
        case BC_OP_BOOL_OR: {
            switch (op) {
            case BC_OP_ADD: printf("add "); break;
            case BC_OP_SUB: printf("sub "); break;
            case BC_OP_MUL: printf("mul "); break;
            case BC_OP_DIV: printf("div "); break;
            case BC_OP_POW: printf("pow "); break;
		    case BC_OP_LESS: printf("less "); break;
		    case BC_OP_GREATER: printf("greater "); break;
		    case BC_OP_EQUAL: printf("equal "); break;
		    case BC_OP_BOOL_AND: printf("booland "); break;
		    case BC_OP_BOOL_OR: printf("boolor "); break;
            }
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            printf("r%u r%u r%u\n", d, a, b);
            break;
        }
        case BC_OP_MOVF: {
            uint8_t d = *bc++;
            float v = *(float*)bc;
            bc += 4;
            printf("movf r%u %f\n", d, v);
            break;
        }
        case BC_OP_SQRT:
        case BC_OP_BOOL_NOT: {
            uint8_t d = *bc++;
            uint8_t v = *bc++;
            switch (op) {
            case BC_OP_SQRT: printf("sqrt r%u r%u\n", d, v); break;
            case BC_OP_BOOL_NOT: printf("boolnot r%u r%u\n", d, v); break;
            }
            break;
        }
        case BC_OP_DELETE: {printf("delete\n"); break;}
        case BC_OP_SEL: {
            uint8_t d = *bc++;
            uint8_t a = *bc++;
            uint8_t b = *bc++;
            uint8_t c = *bc++;
            printf("sel r%u r%u r%u r%u\n", d, a, b, c);
            break;
        }
        case BC_OP_COND_BEGIN: {
			uint8_t c = *bc++;
            uint32_t count = le32toh(*(uint32_t*)bc);
            bc += 4;
            uint8_t regmin = *bc++;
            uint8_t regmax = *bc++;
            printf("beginif r%u (endif at %u) (registers %u-%u)\n", c, (unsigned int)(bc-begin) + count, regmin, regmax);
            break;
        }
        case BC_OP_COND_END: {
            printf("endif\n");
            break;
        }
        case BC_OP_END:
            printf("end\n");
            break;
        }
    }
    assert(bc == end);
}

int main(int argc, char** argv) {
    char* input = NULL;
    char* output = NULL;
    char* type = NULL;
    char* debug_env = getenv("WIP26_COMPILER_DEBUG");
    bool debug = debug_env ? atoi(debug_env) : false;
    
    size_t inc_dir_count = 0;
    char** inc_dirs = NULL;
    
    int opt_index= 0;
    int c = -1;
    while ((c=getopt_long(argc, argv, "i:o:t:I:d", options, &opt_index)) != -1) {
        char** ptr;
        switch (c) {
        case 'd': debug = true; continue;
        case 'i': ptr = &input; goto setstr;
        case 'o': ptr = &output; goto setstr;
        case 't': ptr = &type; goto setstr;
        case 'I': {
            char* dir = copy_str(optarg);
            inc_dirs = append_mem(inc_dirs, inc_dir_count, sizeof(char*), &dir);
            inc_dir_count++;
            continue;
        }
        default: goto error;
        }
        setstr:
            free(*ptr);
            *ptr = copy_str(optarg);
    }
    
    if (!input) {
        fprintf(stderr, "Error: No input file specified.\n");
        goto error;
    }
    
    if (!output) {
        fprintf(stderr, "Error: No output file specified.\n");
        goto error;
    }
    
    if (!type) {
        fprintf(stderr, "Error: No program type specified.\n");
        goto error;
    }
    
    if (strcmp(type, "sim") && strcmp(type, "emit")) {
        fprintf(stderr, "Error: Invalid program type.\n");
        goto error;
    }
    
    ast_t ast;
    char* source = read_file(input);
    if (!source) {
        fprintf(stderr, "Error: Unable to read from %s\n", input);
        goto error;
    }
    if (!parse_program(source, &ast, inc_dir_count, inc_dirs)) {
        fprintf(stderr, "Error: %s\n", ast.error);
        free_ast(&ast);
        free(source);
        goto error;
    }
    free(source);
    
    if (!validate_ast(&ast)) {
        fprintf(stderr, "Error: %s\n", ast.error);
        free_ast(&ast);
        goto error;
    }
    
    if (!add_drop_vars(&ast)) {
        fprintf(stderr, "Error: %s\n", ast.error);
        free_ast(&ast);
        goto error;
    }
    
    if (debug)
        for (size_t i = 0; i < ast.stmt_count; i++)
            print_node(ast.stmts[i], 0);
    
    ir_t ir;
    if (!create_ir(&ast, &ir)) {
        fprintf(stderr, "Error: %s\n", ir.error);
        free_ast(&ast);
        free_ir(&ir);
        goto error;
    }
    
    //remove_redundant_moves(&ir);
    add_drop_insts(&ir);
    
    if (debug) {
        printf("--------IR--------\n");
        for (size_t i = 0; i < ir.inst_count; i++)
            print_inst(&ir, ir.insts[i], 0);
    }
    
    free_ast(&ast);
    
    bc_t bc;
    bc.ir = &ir;
    if (!gen_bc(&bc, !strcmp(type, "sim"))) {
        fprintf(stderr, "Error: %s\n", bc.error);
        free_bc(&bc);
        free_ir(&ir);
        goto error;
    }
    
    if (debug) {
        printf("---BC attributes--\n");
        for (size_t i = 0; i < ir.attr_count; i++)
            for (size_t j = 0; j < ir.attrs[i]->comp; j++)
                printf("%s.%c: load:r%u store:r%u\n",
                       ir.attrs[i]->name.name, "xyzw"[j],
                       bc.attr_load_regs[i*4+j], bc.attr_store_regs[i*4+j]);
        
        printf("----BC uniforms---\n");
        for (size_t i = 0; i < ir.uni_count; i++)
            for (size_t j = 0; j < ir.unis[i]->comp; j++)
                printf("%s.%c: r%u\n",
                       ir.unis[i]->name.name, "xyzw"[j], bc.uni_regs[i*4+j]);
        
        printf("-----Bytecode-----\n");
        print_bc(bc.bc, bc.bc+bc.bc_size);
    }
    
    FILE* dest = fopen(output, "wb");
    if (!dest) {
        fprintf(stderr, "Error: Unable to fopen %s\n", output);
        free_ir(&ir);
        goto error;
    }
    if (!write_bc(dest, &bc)) {
        fprintf(stderr, "Error: %s\n", bc.error);
        free_bc(&bc);
        free_ir(&ir);
        goto error;
    }
    fclose(dest);
    
    free_bc(&bc);
    free_ir(&ir);
    for (size_t i = 0; i < inc_dir_count; i++) free(inc_dirs[i]);
    free(inc_dirs);
    free(input);
    free(output);
    free(type);
    return 0;
    
    error:
        for (size_t i = 0; i < inc_dir_count; i++) free(inc_dirs[i]);
        free(inc_dirs);
        free(input);
        free(output);
        free(type);
        return 1;
}
