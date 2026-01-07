// infix_recur_desc.c - Parsing infix expressions using Recursive Descent.
//
// Build: gcc -o infix_recur_desc infix_recur_desc.c -Wall -Wextra -Wpedantic
//
// Referring to "Parsing Expressions by Recursive Descent" by Theodore Norvell.
// See: <https://www.engr.mun.ca/~theo/Misc/exp_parsing.htm>
//
// Grammar Notation:
// - x?   -> x is optional.
// - [xy] -> match character x or y.
//
// Grammar:
//   EXPR -> TERM ([+-] EXPR)?
//   TERM -> FACT ([*/] TERM)?
//   FACT -> VAL | "(" EXPR ")"
//
// Copyright (C) 2026 Robert Coffey
// Released under the MIT license.

#include <stdio.h>

#define ERROR_IMPL
#include "error.h"

#define LEXER_IMPL
#include "lexer.h"

// AST -------------------------------------------------------------------------

#include <assert.h>

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
        printf(" (%s", ast->tok.word);
        AST_print(ast->left);
        AST_print(ast->right);
        putchar(')');
    }
}

// Classic Recursive Descent ---------------------------------------------------

AST *expr(void);

AST *fact(void) {
    const Token tok = next();
    AST *ast;
    switch (tok.type) {
    case TOK_VAL:
        ast = AST_make(tok, NULL, NULL);
        consume();
        break;
    case TOK_PAREN_L:
        consume();
        ast = expr();
        expect(TOK_PAREN_R);
        break;
    default:
        error("unexpected token: type = %d, word = %s", tok.type, tok.word);
    }
    return ast;
}

AST *term(void) {
    AST *left = fact();
    while (next().type == TOK_TIMES
           || next().type == TOK_DIVIDE) {
        const Token op = next();
        consume();
        AST *right = term();
        left = AST_make(op, left, right);
    }
    return left;
}

AST *expr(void) {
    AST *left = term();
    while (next().type == TOK_PLUS
           || next().type == TOK_MINUS) {
        const Token op = next();
        consume();
        AST *right = expr();
        left = AST_make(op, left, right);
    }
    return left;
}

AST *parse(const char *str) {
    lex_string(str);
    AST *ast = expr();
    expect(TOK_END);
    return ast;
}

// main ------------------------------------------------------------------------

int main(void) {
    // The ASTs of these two expressions should be identical.
    const char *strs[] = {
        "1*2 + 3*(4+5)",
        "(1*2) + (3*(4+5))",
    };
    const size_t str_cnt = sizeof(strs) / sizeof(*strs);
    for (size_t i = 0; i < str_cnt; ++i) {
        const char * const str = strs[i];
        AST *ast = parse(str);
        //printf("Expression: ");
        printf("%-17s =>", str);
        //lex_print();
        AST_print(ast);
        //AST_free(ast); // TODO
        putchar('\n');
    }
    return 0;
}
