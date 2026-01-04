// infix_prec_climb.c - Parsing infix expressions using Precedence Climbing.
//
// Build: gcc -o infix_prec_climb infix_prec_climb.c -Wall -Wextra -Wpedantic
//
// Referring to "Parsing Expressions by Recursive Descent" by Theodore Norvell.
// See: <https://www.engr.mun.ca/~theo/Misc/exp_parsing.htm>
//
// Grammar Notation:
// - x?   -> x is optional.
// - [xy] -> match character x or y.
//
// Copyright (C) 2026 Robert Coffey
// Released under the MIT license.

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void error(const char *fmt, ...) {
    va_list(argptr);
    va_start(argptr, fmt);
    fprintf(stderr, "error: ");
    vfprintf(stderr, fmt, argptr);
    fprintf(stderr, "\n");
    va_end(argptr);
    exit(1);
}

// lexer -----------------------------------------------------------------------

#define DA_APPEND(xs, x)                                                \
    if (xs.count >= xs.capacity) {                                      \
        if (xs.capacity == 0) xs.capacity = 256;                        \
        else                  xs.capacity *= 2;                         \
        xs.items = realloc(xs.items, xs.capacity * sizeof(*xs.items));  \
    }                                                                   \
    xs.items[xs.count++] = x;

typedef enum {
    TOK_PLUS,
    TOK_MINUS,
    TOK_TIMES,
    TOK_DIVIDE,
    TOK_PAREN_L,
    TOK_PAREN_R,
    TOK_OTHER,
    TOK_END,
} TokenType;

typedef struct {
    TokenType type;
    char *word;
} Token;

typedef struct {
    Token *items;
    size_t count;
    size_t capacity;
} Tokens;

void Tokens_free(Tokens toks) {
    for (size_t i = 0; i < toks.count; ++i)
        free(toks.items[i].word);
    free(toks.items);
}

bool Token_is_binary_op(Token tok) {
    return tok.type == TOK_PLUS || tok.type == TOK_MINUS
        || tok.type == TOK_TIMES || tok.type == TOK_DIVIDE;
}

Tokens lex_string(const char *str) {
    Tokens toks = {0};
    while (*str != '\0') {
        // skip whitespace
        while (isspace(*str)) ++str;
        // lex operator
        if (strchr("+-*/()", *str)) {
            TokenType type;
            switch (*str) {
            case '+': type = TOK_PLUS; break;
            case '-': type = TOK_MINUS; break;
            case '*': type = TOK_TIMES; break;
            case '/': type = TOK_DIVIDE; break;
            case '(': type = TOK_PAREN_L; break;
            case ')': type = TOK_PAREN_R; break;
            }
            char *word = calloc(2, sizeof(char));
            word[0] = *str;
            DA_APPEND(toks, ((Token){ type, word }));
            ++str;
        }
        // lex other
        else {
            const char *tok_start = str;
            while (*str && !isspace(*str) && !strchr("+-*/()", *str)) ++str;
            char *word = malloc(str - tok_start);
            strncpy(word, tok_start, str - tok_start);
            DA_APPEND(toks, ((Token){ TOK_OTHER, word }));
        }
    }
    return toks;
}

// AST -------------------------------------------------------------------------

typedef struct AST {
    Token tok;
    struct AST *left, *right;
} AST;

AST *AST_alloc(void) {
    AST *ast = malloc(sizeof(AST));
    if (!ast) error("failed to allocate AST");
    return ast;
}

AST *AST_make(Token tok, AST *left, AST *right) {
    AST *ast = AST_alloc();
    ast->tok = tok;
    ast->left = left;
    ast->right = right;
    return ast;
}

void AST_free(AST *ast) {
    if (ast->left) AST_free(ast->left);
    if (ast->right) AST_free(ast->right);
    free(ast);
}

void AST_print(AST *ast) {
    if (ast->left == NULL || ast->right == NULL) {
        assert(ast->left == ast->right && "both subtrees should be NULL");
        printf(" %s", ast->tok.word);
    } else {
        printf(" (");
        printf("%s", ast->tok.word);
        AST_print(ast->left);
        AST_print(ast->right);
        putchar(')');
    }
}

// Classic Algorithm -----------------------------------------------------------
//
// Grammar for "Classic" Algorithm:
//
//   EXPR -> TERM ([+-] EXPR)?
//   TERM -> FACT ([*/] TERM)?
//   FACT -> NUM | "(" EXPR ")"

Tokens toks = {0};
size_t tok_i = 0;
Token tok_end = { TOK_END, NULL };

Token next(void) {
    if (tok_i < toks.count) return toks.items[tok_i];
    else                    return tok_end;
}

void consume(void) {
    if (tok_i < toks.count) tok_i += 1;
}

void expect(TokenType type) {
    if (type == next().type) consume();
    else                     error("expected %d, got %d", type, next().type);
}

AST *classic_expr(void);

AST *classic_fact(void) {
    const Token tok = next();
    AST *ast;
    switch (tok.type) {
    case TOK_OTHER:
        ast = AST_make(tok, NULL, NULL);
        consume();
        break;
    case TOK_PAREN_L:
        consume();
        ast = classic_expr();
        expect(TOK_PAREN_R);
        break;
    default:
        error("unexpected token: type = %d, word = %s", tok.type, tok.word);
    }
    return ast;
}

AST *classic_term(void) {
    AST *left = classic_fact();
    while (next().type == TOK_TIMES
           || next().type == TOK_DIVIDE) {
        const Token op = next();
        consume();
        AST *right = classic_term();
        left = AST_make(op, left, right);
    }
    return left;
}

AST *classic_expr(void) {
    AST *left = classic_term();
    while (next().type == TOK_PLUS
           || next().type == TOK_MINUS) {
        const Token op = next();
        consume();
        AST *right = classic_expr();
        left = AST_make(op, left, right);
    }
    return left;
}

AST *classic_parse(const char *str) {
    toks = lex_string(str);
    AST *ast = classic_expr();
    expect(TOK_END);
    //Tokens_free(toks); // TODO
    return ast;
}

// Precedence Climbing ---------------------------------------------------------

typedef struct {
    char sym;
    int prec;
} PrecPair;

PrecPair prec_alist[] = {
    { '+', 0 },
    { '-', 0 },
    { '*', 0 },
    { '/', 0 },
};

// main ------------------------------------------------------------------------

int main(void) {
    AST *classic = classic_parse("1 * 2 + 3 * (4 + 5)");
    AST_print(classic);
    putchar('\n');
    return 0;
}
