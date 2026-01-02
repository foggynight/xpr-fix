// infix_prec_climb.c - Parsing infix expressions using Precedence Climbing.
//
// Build: gcc -o infix_prec_climb infix_prec_climb.c -Wall -Wextra -Wpedantic
//
// Referring to "Parsing Expressions by Recursive Descent" by Theodore Norvell.
// See: <https://www.engr.mun.ca/~theo/Misc/exp_parsing.htm>
//
// Copyright (C) 2026 Robert Coffey
// Released under the MIT license.

// Grammar ---------------------------------------------------------------------
//
// Notation:
// - x?   -> x is optional.
// - [xy] -> match character x or y.
//
// Grammar for "Classic" Algorithm:
//
//   EXPR -> TERM ([+-] EXPR)?
//   TERM -> FACT ([*/] EXPR)?
//   FACT -> NUM | "(" EXPR ")"
//
// Grammar for Precedence Climbing Algorithm:
// ...

// lexer -----------------------------------------------------------------------

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

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
    TOK_PLEFT,
    TOK_PRIGHT,
    TOK_OTHER,
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
            case '(': type = TOK_PLEFT; break;
            case ')': type = TOK_PRIGHT; break;
            }
            char *word = calloc(2, sizeof(char));
            word[0] = *str;
            DA_APPEND(toks, ((Token){ type, word }));
            ++str;
        }
        // lex other
        else {
            const char *tok_start = str;
            while (*str && !strchr("+-*/()", *str)) ++str;
            char *word = malloc(str - tok_start);
            strncpy(word, tok_start, str - tok_start);
            DA_APPEND(toks, ((Token){ TOK_OTHER, word }));
        }
    }
    return toks;
}

// Classic Algorithm -----------------------------------------------------------

void next();
void consume();
void expect();

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

#include <stdio.h>

int main(void) {
    Tokens toks = lex_string("1 + 2 * 3");
    puts("Tokens:");
    for (size_t i = 0; i < toks.count; ++i) {
        Token tok = toks.items[i];
        printf("  type = %d, word = %s\n", tok.type, tok.word);
    }
    return 0;
}
