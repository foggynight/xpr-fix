// infix_prec_climb.c - Parsing infix expressions using Precedence Climbing.
//
// Build: gcc -o infix_prec_climb infix_prec_climb.c -Wall -Wextra -Wpedantic
//
// Referring to "Parsing Expressions by Recursive Descent" by Theodore Norvell.
// See: <https://www.engr.mun.ca/~theo/Misc/exp_parsing.htm>
//
// Grammar Notation:
// - {x} -> match x in a loop.
//
// Grammar:
//   EXPR -> E(0)
//   E(p) -> P {B E(q)}
//   P -> U E(q) | "(" EXPR ")" | VAL
//   U -> "-"
//   B -> "=" | "+" | "-" | "*" | "/" | "^"
//
// The loop `{B E(q)}` continues while the next operator is binary and the
// precedence of the operator is greater or equal to p.
// -> What happens when unary "-" is lesser precedence than a binary operator?
//    e.g. "-a+b" => (+ (- a) b) or (- (+ a b))
//
// p and q are precedence values, q is chosen based on the precedence of the
// previous operator:
//   - Unary: q = p (precedence of previous operator)
//   - Binary: q = p + 1, if prev is left-associative
//               = p,     if prev is right-associative
//
// See: `prec_alist` table for operator precedence and associativity.
//
// Copyright (C) 2026 Robert Coffey
// Released under the MIT license.

#include <stdbool.h>
#include <stdio.h>

#define ERROR_IMPL
#include "error.h"

#define LEXER_IMPL
#include "lexer.h"

// AST -------------------------------------------------------------------------

typedef enum {
    AST_VALUE,
    AST_OP_UNARY,
    AST_OP_BINARY,
} ASTType;

typedef struct AST {
    ASTType type;
    Token tok;
    union {
        struct AST *child;      // type = AST_OP_UNARY
        struct {                // type = AST_OP_BINARY
            struct AST *left;
            struct AST *right;
        };
    };
} AST;

AST *AST_alloc(void) {
    AST *ast = malloc(sizeof(AST));
    if (!ast) error("failed to allocate AST");
    return ast;
}

AST *AST_value(Token val) {
    AST *ast = AST_alloc();
    ast->type = AST_VALUE;
    ast->tok = val;
    return ast;
}

AST *AST_unary(Token op, AST *child) {
    AST *ast = AST_alloc();
    ast->type = AST_OP_UNARY;
    ast->tok = op;
    ast->child = child;
    return ast;
}

AST *AST_binary(Token op, AST *left, AST *right) {
    AST *ast = AST_alloc();
    ast->type = AST_OP_BINARY;
    ast->tok = op;
    ast->left = left;
    ast->right = right;
    return ast;
}

void AST_print_(AST *ast) {
    switch (ast->type) {
    case AST_VALUE:
        printf(" %s", ast->tok.word);
        break;
    case AST_OP_UNARY:
        printf(" (%s", ast->tok.word);
        AST_print_(ast->child);
        putchar(')');
        break;
    case AST_OP_BINARY:
        printf(" (%s", ast->tok.word);
        AST_print_(ast->left);
        AST_print_(ast->right);
        putchar(')');
        break;
    }
}

void AST_print(AST *ast) {
    //printf(" AST:");
    AST_print_(ast);
}

// Precedence Climbing Algorithm -----------------------------------------------

typedef enum {
    ASSOC_NULL,
    ASSOC_LEFT,
    ASSOC_RIGHT,
} Assoc;

typedef struct {
    char sym;
    int prec;
    Assoc assoc;
} PrecItem;

PrecItem unary_prec_arr[] = {
    { '-', 4, ASSOC_NULL},
};
size_t unary_prec_cnt = sizeof(unary_prec_arr) / sizeof(*unary_prec_arr);

PrecItem binary_prec_arr[] = {
    { '=', 0, ASSOC_LEFT },
    { '+', 1, ASSOC_LEFT },
    { '-', 1, ASSOC_LEFT },
    { '*', 2, ASSOC_LEFT },
    { '/', 2, ASSOC_LEFT },
    { '^', 3, ASSOC_RIGHT },
};
size_t binary_prec_cnt = sizeof(binary_prec_arr) / sizeof(*binary_prec_arr);

PrecItem *prec_arr_find(PrecItem *prec_arr, size_t prec_arr_cnt, char sym) {
    for (size_t i = 0; i < prec_arr_cnt; ++i) {
        if (prec_arr[i].sym == sym) {
            return prec_arr + i;
        }
    }
    return NULL;
}

bool prec_arr_contains(PrecItem *prec_arr, size_t prec_arr_cnt, char sym) {
    return prec_arr_find(prec_arr, prec_arr_cnt, sym) != NULL;
}

bool is_unary_op(Token tok) {
    if (tok.type == TOK_END) return false;
    return prec_arr_contains(unary_prec_arr, unary_prec_cnt, *tok.word);
}

bool is_binary_op(Token tok) {
    if (tok.type == TOK_END) return false;
    return prec_arr_contains(binary_prec_arr, binary_prec_cnt, *tok.word);
}

int get_unary_prec(Token tok) {
    PrecItem *item = prec_arr_find(unary_prec_arr, unary_prec_cnt, *tok.word);
    if (item) return item->prec;
    error("invalid unary operator: %c", *tok.word);
    __builtin_unreachable();
}

int get_binary_prec(Token tok) {
    PrecItem *item = prec_arr_find(binary_prec_arr, binary_prec_cnt, *tok.word);
    if (item) return item->prec;
    error("invalid binary operator: %c", *tok.word);
    __builtin_unreachable();
}

AST *expr(void);
AST *E(int prev_prec);

AST *P(void) {
    if (is_unary_op(next())) {
        const Token op = next();
        consume();
        AST *child = E(get_unary_prec(op));
        return AST_unary(op, child);
    }
    else if (next().type == TOK_PAREN_L) {
        consume();
        AST *ast = expr();
        expect(TOK_PAREN_R);
        return ast;
    }
    else if (next().type == TOK_VAL) {
        const Token val = next();
        consume();
        return AST_value(val);
    }
    else {
        error("invalid token: %s", next().word);
    }
    __builtin_unreachable();
}

AST *E(int min_prec) {
    AST *left = P();
    while (is_binary_op(next()) && get_binary_prec(next()) >= min_prec) {
        const Token op = next();
        consume();
        const PrecItem *item =
            prec_arr_find(binary_prec_arr, binary_prec_cnt, *op.word);
        const int next_prec =
            item->assoc == ASSOC_LEFT ? item->prec + 1 : item->prec;
        AST *right = E(next_prec);
        left = AST_binary(op, left, right);
    }
    return left;
}

AST *expr(void) { return E(0); }

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
        "1^-2^3*4 + -5*6*-7",
        "((1^((-2)^3))*4) + (((-5)*6)*-7)",
    };
    const size_t str_cnt = sizeof(strs) / sizeof(*strs);
    for (size_t i = 0; i < str_cnt; ++i) {
        const char * const str = strs[i];
        AST *ast = parse(str);
        //printf("Expression: ");
        printf("%-32s =>", str);
        //lex_print();
        AST_print(ast);
        //AST_free(ast); // TODO
        putchar('\n');
    }
    return 0;
}
