#include "cpp-utils.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#ifdef __GNUC__
// enable compile time checking of format arguments
void
    __attribute__((format(printf, 1, 0)))
    fatal(const char *fmt, ...) {
#else
void fatal(const char *fmt, ...) {
#endif
    fprintf(stderr, "fatal: ");
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    exit(EXIT_FAILURE);
}