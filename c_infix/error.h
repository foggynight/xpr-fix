// error.h - `error` function used by the parsers in this directory.
//
// Usage:
// - `#define ERROR_IMPL`
// - `#include "error.h"`
//
// Copyright (C) 2026 Robert Coffey
// Released under the MIT license.

#ifndef ERROR_H
#define ERROR_H

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

void error(const char *fmt, ...);

#ifdef ERROR_IMPL

void error(const char *fmt, ...) {
    va_list(argptr);
    va_start(argptr, fmt);
    fprintf(stderr, "error: ");
    vfprintf(stderr, fmt, argptr);
    fprintf(stderr, "\n");
    va_end(argptr);
    exit(1);
}

#endif // ERROR_IMPL
#endif // ERROR_H
