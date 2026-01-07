// lexer.h - Lexer used by the parsers in this directory.
//
// Usage:
// - `#define LEXER_IMPL`
// - `#include "lexer.h"`
//
// Copyright (C) 2026 Robert Coffey
// Released under the MIT license.

#ifndef LEXER_H
#define LEXER_H

#include <stdlib.h>

typedef enum {
    TOK_VAL,
    TOK_EQUALS,
    TOK_PLUS,
    TOK_MINUS,
    TOK_TIMES,
    TOK_DIVIDE,
    TOK_EXPONENT,
    TOK_PAREN_L,
    TOK_PAREN_R,
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

void lex_string(const char *str);
void lex_print(void); // print lexed tokens

Token next(void);
void consume(void);
void expect(TokenType type);

#ifdef LEXER_IMPL

#include <ctype.h>
#include <string.h>

#define DA_APPEND(xs, x)                                                \
    if (xs.count >= xs.capacity) {                                      \
        if (xs.capacity == 0) xs.capacity = 256;                        \
        else                  xs.capacity *= 2;                         \
        xs.items = realloc(xs.items, xs.capacity * sizeof(*xs.items));  \
    }                                                                   \
    xs.items[xs.count++] = x;

static const char *op_chars = "=+-*/^()";

static Tokens toks = {0};
static size_t next_i = 0;
static const Token tok_end = { TOK_END, NULL };

static void Tokens_destroy(Tokens *toks) {
    for (size_t i = 0; i < toks->count; ++i)
        free(toks->items[i].word);
    free(toks->items);
    toks->items = NULL;
    toks->count = 0;
    toks->capacity = 0;
}

static void Tokens_print(Tokens toks) {
    puts("Tokens:");
    for (size_t i = 0; i < toks.count; ++i) {
        const Token tok = toks.items[i];
        printf("  type = %d, word = %s\n", tok.type, tok.word);
    }
}

static void lex_reset(void) {
    Tokens_destroy(&toks);
    next_i = 0;
}

void lex_string(const char *str) {
    lex_reset();
    while (*str != '\0') {
        // skip whitespace
        while (isspace(*str)) ++str;
        // lex operator
        if (strchr(op_chars, *str)) {
            TokenType type;
            switch (*str) {
            case '=': type = TOK_EQUALS; break;
            case '+': type = TOK_PLUS; break;
            case '-': type = TOK_MINUS; break;
            case '*': type = TOK_TIMES; break;
            case '/': type = TOK_DIVIDE; break;
            case '^': type = TOK_EXPONENT; break;
            case '(': type = TOK_PAREN_L; break;
            case ')': type = TOK_PAREN_R; break;
            }
            char *word = calloc(2, sizeof(char));
            word[0] = *str;
            DA_APPEND(toks, ((Token){ type, word }));
            ++str;
        }
        // lex value
        else {
            const char *tok_start = str;
            while (*str && !isspace(*str) && !strchr(op_chars, *str)) ++str;
            const size_t word_len = str - tok_start;
            char *word = malloc(word_len + 1);
            strncpy(word, tok_start, word_len);
            word[word_len] = '\0';
            DA_APPEND(toks, ((Token){ TOK_VAL, word }));
        }
    }
}

void lex_print(void) { Tokens_print(toks); }

Token next(void) {
    if (next_i < toks.count) return toks.items[next_i];
    else                     return tok_end;
}

void consume(void) {
    if (next_i < toks.count) next_i += 1;
}

void expect(TokenType type) {
    if (type == next().type) {
        consume();
    } else {
        error("expected %d, got %d", type, next().type);
        __builtin_unreachable();
    }
}

#endif // LEXER_IMPL
#endif // LEXER_H
