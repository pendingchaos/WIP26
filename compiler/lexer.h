#ifndef LEXER_H
#define LEXER_H
#include <stdbool.h>

typedef enum token_type_t {
    TOKT_EOF,
    TOKT_NUM,
    TOKT_ID,
    TOKT_ASSIGN,
    TOKT_ADD,
    TOKT_SUB,
    TOKT_MUL,
    TOKT_DIV,
    TOKT_POW,
    TOKT_DOT,
    TOKT_VAR,
    TOKT_PROP,
    TOKT_COLON,
    TOKT_SEMICOLON,
    TOKT_LEFT_PAREN,
    TOKT_RIGHT_PAREN,
    TOKT_LEFT_BRACE,
    TOKT_RIGHT_BRACE,
    TOKT_RETURN,
    TOKT_COMMA,
    TOKT_FUNC,
    TOKT_IF,
    TOKT_LESS,
    TOKT_GREATER,
    TOKT_EQUAL
} token_type_t;

typedef struct tokens_t tokens_t;
typedef struct token_t token_t;
typedef struct ast_t ast_t;

struct token_t {
    token_type_t type;
    const char* begin;
    const char* end;
    unsigned int line;
    unsigned int column;
};

struct tokens_t {
    const char* src;
    ast_t* ast;
    unsigned int cur_line;
    const char* cur_line_ptr;
};

void create_tokens(tokens_t* tokens, ast_t* ast, const char* src);
const char* get_tok_type_str(token_type_t type);
bool peek_token(tokens_t* toks, token_t* tok);
bool get_token(tokens_t* toks, token_t* tok);
bool expect_token(tokens_t* toks, token_type_t type, token_t* tok);
#endif
