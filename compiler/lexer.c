#include "lexer.h"
#include "ast.h"
#include "shared.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

static bool is_digit(char c) {
    return c>='0' && c<='9';
}

static bool is_alpha(char c) {
    return (c>='a' && c<='z') || (c>='A' && c<='Z');
}

static bool is_space(char c) {
    return c==' ' || c=='\n' || c=='\t';
}

void create_tokens(tokens_t* tokens, ast_t* ast, const char* src) {
    tokens->src = src;
    tokens->ast = ast;
    tokens->cur_line = 1;
    tokens->cur_line_ptr = src;
}

const char* get_tok_type_str(token_type_t type) {
    switch (type) {
    case TOKT_EOF: return "end-of-file";
    case TOKT_NUM: return "number";
    case TOKT_ID: return "identifier";
    case TOKT_ASSIGN: return "'='";
    case TOKT_ADD: return "'+'";
    case TOKT_SUB: return "'-'";
    case TOKT_MUL: return "'*'";
    case TOKT_DIV: return "'/'";
    case TOKT_POW: return "'^'";
    case TOKT_DOT: return "'.'";
    case TOKT_VAR: return "'var'";
    case TOKT_ATTR: return "'attribute'";
    case TOKT_UNI: return "'uniform'";
    case TOKT_COLON: return "':'";
    case TOKT_SEMICOLON: return "';'";
    case TOKT_LEFT_PAREN: return "'('";
    case TOKT_RIGHT_PAREN: return "')'";
    case TOKT_LEFT_BRACE: return "'{'";
    case TOKT_RIGHT_BRACE: return "'}'";
    case TOKT_RETURN: return "'return'";
    case TOKT_COMMA: return "','";
    case TOKT_FUNC: return "func";
    case TOKT_IF: return "if";
    case TOKT_WHILE: return "while";
    case TOKT_FOR: return "for";
    case TOKT_LESS: return "<";
    case TOKT_GREATER: return ">";
    case TOKT_EQUAL: return "==";
    case TOKT_BOOL_AND: return "&&";
    case TOKT_BOOL_OR: return "||";
    case TOKT_BOOL_NOT: return "!";
    }
    assert(false);
}

static bool _token(tokens_t* toks, token_t* tok, bool get) {
    const char* src = toks->src;
    
    //Skip whitespace
    while (true) {
        if (!*src) {
            tok->type = TOKT_EOF;
            tok->begin = tok->end = src + strlen(src);
            return true;
        }
        if (*src=='\n' && get) {
            toks->cur_line++;
            toks->cur_line_ptr = src++;
        } else if (*src=='\n' || is_space(*src)) src++;
        else break;
    }
    
    tok->begin = src;
    tok->loc.column = src - toks->cur_line_ptr;
    tok->loc.line = toks->cur_line;
    tok->end = src;
    #define CHAR_TOK(c, t) else if (*tok->begin == c) {tok->type = t; tok->end = src + 1;}
    #define TWO_CHAR_TOK(c1, c2, t) else if ((*tok->begin)==c1 && (*(tok->begin+1))==c2) {tok->type = t; tok->end = src + 2;}
    if (false) {
    } TWO_CHAR_TOK('=', '=', TOKT_EQUAL)
    TWO_CHAR_TOK('&', '&', TOKT_BOOL_AND)
    TWO_CHAR_TOK('|', '|', TOKT_BOOL_OR)
    CHAR_TOK('!', TOKT_BOOL_NOT)
    CHAR_TOK('=', TOKT_ASSIGN)
    CHAR_TOK('+', TOKT_ADD)
    CHAR_TOK('-', TOKT_SUB)
    CHAR_TOK('*', TOKT_MUL)
    CHAR_TOK('/', TOKT_DIV)
    CHAR_TOK('^', TOKT_POW)
    CHAR_TOK('.', TOKT_DOT)
    CHAR_TOK(':', TOKT_COLON)
    CHAR_TOK(';', TOKT_SEMICOLON)
    CHAR_TOK('(', TOKT_LEFT_PAREN)
    CHAR_TOK(')', TOKT_RIGHT_PAREN)
    CHAR_TOK('{', TOKT_LEFT_BRACE)
    CHAR_TOK('}', TOKT_RIGHT_BRACE)
    CHAR_TOK(',', TOKT_COMMA)
    CHAR_TOK('<', TOKT_LESS)
    CHAR_TOK('>', TOKT_GREATER)
    else if (*src == '#') {
        while (true) {
            char c = *toks->src++;
            if (c == '\n' && get) {
                toks->cur_line++;
                toks->cur_line_ptr = toks->src;
                break;
            } else if (c=='\n' || !c)
                break;
        }
        return _token(toks, tok, get);
    } else if (is_digit(*tok->begin)) {
        tok->type = TOKT_NUM;
        strtod(tok->begin, (char**)&tok->end);
        
        if (tok->end==tok->begin) {
            set_error(toks->ast, "%u:%u: Unable to parse number.", tok->loc.line, tok->loc.column);
            return false;
        }
    } else if (is_alpha(*tok->begin) || *tok->begin=='_') {
        tok->type = TOKT_ID;
        while (1) {
            char c = *tok->end++;
            if (!(is_digit(c) || is_alpha(c) || c=='_')) break;
        }
        tok->end--;
        
        size_t len = tok->end-tok->begin;
        if (len==3 && !strncmp(tok->begin, "var", len))
            tok->type = TOKT_VAR;
        else if (len==9 && !strncmp(tok->begin, "attribute", len))
            tok->type = TOKT_ATTR;
        else if (len==7 && !strncmp(tok->begin, "uniform", len))
            tok->type = TOKT_UNI;
        else if (len==6 && !strncmp(tok->begin, "return", len))
            tok->type = TOKT_RETURN;
        else if (len==4 && !strncmp(tok->begin, "func", len))
            tok->type = TOKT_FUNC;
        else if (len==2 && !strncmp(tok->begin, "if", len))
            tok->type = TOKT_IF;
        else if (len==5 && !strncmp(tok->begin, "while", len))
            tok->type = TOKT_WHILE;
        else if (len==3 && !strncmp(tok->begin, "for", len))
            tok->type = TOKT_FOR;
    } else {
        set_error(toks->ast, "%u:%u: Unexpected character: '%c'", tok->loc.line, tok->loc.column, *src);
        return false;
    }
    
    if (get) toks->src = tok->end;
    
    return true;
}

bool peek_token(tokens_t* toks, token_t* tok) {
    return _token(toks, tok, false);
}

bool get_token(tokens_t* toks, token_t* tok) {
    return _token(toks, tok, true);
}

bool expect_token(tokens_t* toks, token_type_t type, token_t* tok) {
    if (!get_token(toks, tok)) return false;
    if (tok->type != type)
        return set_error(toks->ast, "%u:%u: Expected %s. Got %s.", tok->loc.line, tok->loc.column, get_tok_type_str(type), get_tok_type_str(tok->type));
    return true;
}
