#include "parser.h"
#include "lexer.h"
#include "shared.h"

#include <stdlib.h>
#include <string.h>

static node_t* parse_expr(tokens_t* toks, token_type_t delim1, token_type_t delim2);

static node_t* parse_primary(tokens_t* toks) {
    token_t tok;
    if (!get_token(toks, &tok)) return NULL;
    switch (tok.type) {
    case TOKT_NUM: {
        return (node_t*)create_num_node(toks->ast, atof(tok.begin));
    }
    case TOKT_ID: {
        char name[tok.end-tok.begin+1];
        strncpy(name, tok.begin, tok.end-tok.begin);
        name[tok.end-tok.begin] = 0;
        
        token_t peek;
        if (!peek_token(toks, &peek)) return NULL;
        if (peek.type==TOKT_LEFT_PAREN) {
            get_token(toks, &peek);
            size_t arg_count = 0;
            node_t** args = NULL;
            peek_token(toks, &peek);
            if (peek.type != TOKT_RIGHT_PAREN) {
                while (true) {
                    node_t* arg = parse_expr(toks, TOKT_RIGHT_PAREN, TOKT_COMMA);
                    if (!arg) goto error;
                    
                    args = append_mem(args, arg_count++, sizeof(node_t*), &arg);
                    arg = NULL;
                    
                    token_t tok;
                    if (!get_token(toks, &tok)) goto error;
                    if (tok.type == TOKT_RIGHT_PAREN) break;
                    else if (tok.type != TOKT_COMMA)  {
                        set_error(toks->ast, "%u:%u: Expected ')' or ','. Got %s.", tok.line, tok.column, get_tok_type_str(tok.type));
                        goto error;
                    } else continue;
                    
                    error:
                        free(args);
                        return NULL;
                }
            } else {
                get_token(toks, &peek);
            }
            
            node_t* node = (node_t*)create_call_node(toks->ast, name, arg_count, args);
            free(args);
            return node;
        } else {
            return (node_t*)create_id_node(toks->ast, name);
        }
    }
    case TOKT_SUB: {
        return (node_t*)create_unary_node(toks->ast, NODET_NEG, parse_primary(toks));
    }
    case TOKT_LEFT_PAREN: {
        node_t* res = (node_t*)parse_expr(toks, TOKT_RIGHT_PAREN, TOKT_RIGHT_PAREN);
        token_t tok;
        if (!expect_token(toks, TOKT_RIGHT_PAREN, &tok)) return NULL;
        return res;
    }
    default: {
        set_error(toks->ast, "%u:%u: Expected primary expression. Got %s.", tok.line, tok.column, get_tok_type_str(tok.type));
        return NULL;
    }
    }
}

static int get_precedence(token_type_t tok) {
    switch (tok) {
    case TOKT_ASSIGN: return 0;
    case TOKT_LESS:
    case TOKT_GREATER:
    case TOKT_EQUAL: return 1;
    case TOKT_ADD:
    case TOKT_SUB: return 2;
    case TOKT_MUL:
    case TOKT_DIV: return 3;
    case TOKT_POW: return 4;
    case TOKT_DOT: return 5;
    default: return -1;
    }
}

static bool is_right_associative(token_type_t tok) {
    return tok == TOKT_ASSIGN;
}

static bool is_binary_op(token_type_t tok) {
    switch (tok) {
    case TOKT_ASSIGN:
    case TOKT_ADD:
    case TOKT_SUB:
    case TOKT_MUL:
    case TOKT_DIV:
    case TOKT_POW:
    case TOKT_LESS:
    case TOKT_GREATER:
    case TOKT_EQUAL:
    case TOKT_DOT: return true;
    default: return false;
    }
}

static node_t* create_node(ast_t* ast, token_type_t tok, node_t* lhs, node_t* rhs) {
    switch (tok) {
    case TOKT_ASSIGN: return (node_t*)create_bin_node(ast, NODET_ASSIGN, lhs, rhs);
    case TOKT_ADD: return (node_t*)create_bin_node(ast, NODET_ADD, lhs, rhs);
    case TOKT_SUB: return (node_t*)create_bin_node(ast, NODET_SUB, lhs, rhs);
    case TOKT_MUL: return (node_t*)create_bin_node(ast, NODET_MUL, lhs, rhs);
    case TOKT_DIV: return (node_t*)create_bin_node(ast, NODET_DIV, lhs, rhs);
    case TOKT_POW: return (node_t*)create_bin_node(ast, NODET_POW, lhs, rhs);
    case TOKT_LESS: return (node_t*)create_bin_node(ast, NODET_LESS, lhs, rhs);
    case TOKT_GREATER: return (node_t*)create_bin_node(ast, NODET_GREATER, lhs, rhs);
    case TOKT_EQUAL: return (node_t*)create_bin_node(ast, NODET_EQUAL, lhs, rhs);
    case TOKT_DOT: return (node_t*)create_bin_node(ast, NODET_MEMBER, lhs, rhs);
    default: return NULL;
    }
}

static node_t* _parse_expr(tokens_t* toks, token_type_t delim1, token_type_t delim2, node_t* lhs, int min_prec) {
    token_t lookahead;
    if (!peek_token(toks, &lookahead)) return NULL;
    
    while (is_binary_op(lookahead.type) ? (get_precedence(lookahead.type)>=min_prec) : false) {
        token_t op = lookahead;
        if (!get_token(toks, &lookahead)) return NULL;
        node_t* rhs = parse_primary(toks);
        if (!peek_token(toks, &lookahead) || !rhs) return NULL;
        
        if (lookahead.type==TOKT_EOF || lookahead.type==delim1 ||
            lookahead.type==delim2)
            return create_node(toks->ast, op.type, lhs, rhs);
        
        int lookahead_prec = get_precedence(lookahead.type);
        int op_prec = get_precedence(op.type);
        
        while ((is_binary_op(lookahead.type) ? lookahead_prec>op_prec : false) ||
               (is_right_associative(lookahead.type)&&lookahead_prec==op_prec)) {
            rhs = _parse_expr(toks, delim1, delim2, rhs, lookahead_prec);
            if (!peek_token(toks, &lookahead)) return NULL;
            if (lookahead.type==TOKT_EOF || lookahead.type==delim1 ||
                lookahead.type==delim2)
                return create_node(toks->ast, op.type, lhs, rhs);
            
            lookahead_prec = get_precedence(lookahead.type);
        }
        
        lhs = create_node(toks->ast, op.type, lhs, rhs);
    }
    
    return lhs;
}

static node_t* parse_expr(tokens_t* toks, token_type_t delim1, token_type_t delim2) {
    node_t* primary = parse_primary(toks);
    if (!primary) return NULL;
    return _parse_expr(toks, delim1, delim2, primary, -1);
}

static node_t* parse_decl(tokens_t* toks, token_type_t tok_type, node_type_t node_type) {
    token_t tok;
    if (!expect_token(toks, tok_type, &tok)) return NULL;
    
    token_t name_tok;
    if (!expect_token(toks, TOKT_ID, &name_tok)) return NULL;
    
    token_t colon_tok;
    if (!expect_token(toks, TOKT_COLON, &colon_tok)) return NULL;
    
    token_t dtype_tok;
    if (!expect_token(toks, TOKT_ID, &dtype_tok)) return NULL;
    
    char name[name_tok.end-name_tok.begin+1];
    strncpy(name, name_tok.begin, name_tok.end-name_tok.begin);
    name[name_tok.end-name_tok.begin] = 0;
    
    char dtype[dtype_tok.end-dtype_tok.begin+1];
    strncpy(dtype, dtype_tok.begin, dtype_tok.end-dtype_tok.begin);
    dtype[dtype_tok.end-dtype_tok.begin] = 0;
    
    node_t* decl = (node_t*)create_decl_node(toks->ast, node_type, name, dtype);
    
    token_t peek;
    if (peek_token(toks, &peek) && peek.type==TOKT_ASSIGN) {
        get_token(toks, &peek);
        node_t* val = parse_expr(toks, TOKT_SEMICOLON, TOKT_SEMICOLON);
        return (node_t*)create_bin_node(toks->ast, NODET_ASSIGN, decl, val);
    }
    
    return decl;
}

static node_t* parse_var_decl(tokens_t* toks) {
    return parse_decl(toks, TOKT_VAR, NODET_VAR_DECL);
}

static node_t* parse_prop_decl(tokens_t* toks) {
    return parse_decl(toks, TOKT_PROP, NODET_PROP_DECL);
}

static node_t* parse_stmt(tokens_t* toks, bool* semicolon_req, size_t* stmt_count, node_t*** stmts, size_t inc_dir_count, char** inc_dirs);
static bool parse_stmts(tokens_t* toks, size_t* count_, node_t*** stmts_, bool in_braces, size_t inc_dir_count, char** inc_dirs) {
    size_t count = 0;
    node_t** stmts = NULL;
    while (true) {
        token_t tok;
        if (!peek_token(toks, &tok)) goto error;
        if (tok.type == TOKT_EOF) break;
        else if (tok.type==TOKT_RIGHT_BRACE && in_braces) {
            if (!get_token(toks, &tok)) goto error;
            break;
        }
        
        bool semicolon_req;
        node_t* stmt = parse_stmt(toks, &semicolon_req, &count, &stmts, inc_dir_count, inc_dirs);
        if (!stmt) goto error;
        
        stmts = append_mem(stmts, count++, sizeof(node_t*), &stmt);
        stmt = NULL;
        
        if (semicolon_req && !expect_token(toks, TOKT_SEMICOLON, &tok))
            goto error;
        continue;
        error:
            free(stmts);
            return false;
    }
    
    *stmts_ = stmts;
    *count_ = count;
    
    return true;
}

static node_t* parse_func_decl(tokens_t* toks, size_t inc_dir_count, char** inc_dirs) {
    token_t func_tok;
    if (!expect_token(toks, TOKT_FUNC, &func_tok)) return NULL;
    
    token_t name_tok;
    if (!expect_token(toks, TOKT_ID, &name_tok)) return NULL;
    
    token_t left_paren_tok;
    if (!expect_token(toks, TOKT_LEFT_PAREN, &left_paren_tok)) return NULL;
    
    size_t arg_count = 0;
    char** arg_names = NULL;
    char** arg_dtypes = NULL;
    while (true) {
        char* name = NULL;
        char* dtype = NULL;
        
        token_t tok;
        if (!get_token(toks, &tok)) goto error;
        if (tok.type == TOKT_RIGHT_PAREN) break;
        if (tok.type != TOKT_ID) {
            set_error(toks->ast, "%u:%u: Expected ')' or identifier. Got %s.\n", tok.line, tok.column, get_tok_type_str(tok.type));
            goto error;
        }
        
        token_t colon_tok;
        if (!expect_token(toks, TOKT_COLON, &colon_tok)) goto error;
        
        token_t type_tok;
        if (!expect_token(toks, TOKT_ID, &type_tok)) goto error;
        
        name = alloc_mem(tok.end-tok.begin+1);
        dtype = alloc_mem(type_tok.end-type_tok.begin+1);
        strncpy(name, tok.begin, tok.end-tok.begin);
        strncpy(dtype, type_tok.begin, type_tok.end-type_tok.begin);
        
        arg_names = append_mem(arg_names, arg_count, sizeof(char*), &name);
        arg_dtypes = append_mem(arg_dtypes, arg_count++, sizeof(char*), &dtype);
        name = NULL;
        dtype = NULL;
        
        if (!peek_token(toks, &tok)) goto error;
        if (tok.type == TOKT_COMMA)
            get_token(toks, &tok);
        
        continue;
        error:
            for (size_t i = 0; i < arg_count; i++) {
                free(arg_names[i]);
                free(arg_dtypes[i]);
            }
            free(name);
            free(dtype);
            free(arg_names);
            free(arg_dtypes);
            return NULL;
    }
    
    char* rtype = NULL;
    char* name = NULL;
    
    token_t colon_tok;
    if (!expect_token(toks, TOKT_COLON, &colon_tok)) goto error2;
    
    token_t rtype_tok;
    if (!expect_token(toks, TOKT_ID, &rtype_tok)) goto error2;
    
    token_t left_brace_tok;
    if (!expect_token(toks, TOKT_LEFT_BRACE, &left_brace_tok)) goto error2;
    
    size_t stmt_count = 0;
    node_t** stmts = NULL;
    if (!parse_stmts(toks, &stmt_count, &stmts, true, inc_dir_count, inc_dirs)) goto error;
    
    rtype = alloc_mem(rtype_tok.end-rtype_tok.begin+1);
    name = alloc_mem(name_tok.end-name_tok.begin+1);
    strncpy(rtype, rtype_tok.begin, rtype_tok.end-rtype_tok.begin);
    strncpy(name, name_tok.begin, name_tok.end-name_tok.begin);
    
    func_decl_node_t decl;
    decl.name = name;
    decl.arg_count = arg_count;
    decl.arg_names = arg_names;
    decl.arg_types = arg_dtypes;
    decl.stmt_count = stmt_count;
    decl.stmts = stmts;
    decl.ret_type = rtype;
    node_t* res = (node_t*)create_func_decl_node(toks->ast, &decl);
    free(name);
    free(rtype);
    for (size_t i = 0; i < arg_count; i++) {
        free(arg_names[i]);
        free(arg_dtypes[i]);
    }
    free(arg_names);
    free(arg_dtypes);
    free(stmts);
    return res;
    
    error2:
        free(name);
        free(stmts);
        free(rtype);
        for (size_t i = 0; i < arg_count; i++) {
            free(arg_names[i]);
            free(arg_dtypes[i]);
        }
        free(arg_names);
        free(arg_dtypes);
        return NULL;
}

static node_t* parse_return(tokens_t* toks) {
    token_t tok;
    if (!get_token(toks, &tok)) return NULL;
    return (node_t*)create_unary_node(toks->ast, NODET_RETURN, parse_expr(toks, TOKT_EOF, TOKT_EOF));
}

static node_t* parse_include(tokens_t* toks, size_t* stmt_count, node_t*** stmts, size_t inc_dir_count, char** inc_dirs) {
    token_t tok;
    get_token(toks, &tok);
    peek_token(toks, &tok);
    const char* begin = tok.begin;
    toks->src = begin;
    while (true) {
        char c = *toks->src;
        if (c == ';') break;
        toks->src++;
    }
    const char* end = toks->src;
    
    char* filename = alloc_mem(end-begin+1);
    strncpy(filename, begin, end-begin);
    
    char* source = NULL;
    for (size_t i = 0; i < inc_dir_count; i++) {
        char* fname = alloc_mem(strlen(inc_dirs[i])+strlen(filename)+1);
        strcpy(fname, inc_dirs[i]);
        strcat(fname, filename);
        source = read_file(fname);
        free(fname);
        if (source) break;
    }
    if (!source) {
        set_error(toks->ast, "%u:%u: Failed to read from %s", tok.line, tok.column, filename);
        free(filename);
        return NULL;
    }
    free(filename);
    
    ast_t* ast = toks->ast;
    
    tokens_t toks2;
    create_tokens(&toks2, ast, source);
    
    size_t new_stmt_count;
    node_t** new_stmts;
    if (!parse_stmts(&toks2, &new_stmt_count, &new_stmts, false, inc_dir_count, inc_dirs)) {
        free(source);
        return NULL;
    }
    free(source);
    
    *stmts = realloc_mem(*stmts, (*stmt_count+new_stmt_count)*sizeof(node_t*));
    memcpy(*stmts+*stmt_count, new_stmts, new_stmt_count*sizeof(node_t*));
    free(new_stmts);
    *stmt_count += new_stmt_count;
    
    return create_nop_node(ast);
}

static node_t* parse_if(tokens_t* toks, size_t inc_dir_count, char** inc_dirs) {
    token_t if_tok;
    if (!expect_token(toks, TOKT_IF, &if_tok)) return NULL;
    
    node_t* cond = parse_expr(toks, TOKT_LEFT_BRACE, TOKT_LEFT_BRACE);
    
    token_t left_brace_tok;
    if (!expect_token(toks, TOKT_LEFT_BRACE, &left_brace_tok)) return NULL;
    
    size_t stmt_count = 0;
    node_t** stmts = NULL;
    if (!parse_stmts(toks, &stmt_count, &stmts, true, inc_dir_count, inc_dirs)) return NULL;
    
    node_t* res = (node_t*)create_if_node(toks->ast, stmt_count, stmts, cond);
    free(stmts);
    return res;
}

static node_t* parse_stmt(tokens_t* toks, bool* semicolon_req, size_t* stmt_count, node_t*** stmts, size_t inc_dir_count, char** inc_dirs) {
    *semicolon_req = true;
    token_t tok;
    if (!peek_token(toks, &tok)) return NULL;
    switch (tok.type) {
    case TOKT_EOF: return (node_t*)create_nop_node(toks->ast);
    case TOKT_VAR: return parse_var_decl(toks);
    case TOKT_PROP: return parse_prop_decl(toks);
    case TOKT_FUNC: return *semicolon_req=false, parse_func_decl(toks, inc_dir_count, inc_dirs);
    case TOKT_RETURN: return parse_return(toks);
    case TOKT_IF: return *semicolon_req=false, parse_if(toks, inc_dir_count, inc_dirs);
    case TOKT_ID:
        if (!strncmp(tok.begin, "include", 7) && (tok.end-tok.begin)==7)
            return parse_include(toks, stmt_count, stmts, inc_dir_count, inc_dirs);
    default: return parse_expr(toks, TOKT_SEMICOLON, TOKT_SEMICOLON);
    }
}

bool parse_program(const char* src, ast_t* ast, size_t inc_dir_count, char** inc_dirs) {
    ast->error[0] = 0;
    ast->stmt_count = 0;
    ast->stmts = NULL;
    ast->mem = create_mem_group();
    
    tokens_t toks;
    create_tokens(&toks, ast, src);
    
    if (!parse_stmts(&toks, &ast->stmt_count, &ast->stmts, false, inc_dir_count, inc_dirs))
        return false;
    
    return true;
}
