/* sample.c - exercises every feature of mypreprocessor
 * multi-line comment
 */
#include <stdio.h>        // system header: not found locally, kept as is
#include <mylib.h>        // found next to this file, inlined
#include "mylib.h"        // second include: guarded, adds nothing

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define AREA(r) (PI * SQUARE(r))
#define DEBUG 1
#define STR(x) #x
#define GREETING "PI // not a comment /* nor this */"

int main(void)
{
    double r = 2.0;          // single-line comment
    double a = AREA(r);      /* inline block comment */
    int m = MAX(SQUARE(3), 10);
    int PIE = 1;             /* identifiers containing PI must not change */
    printf("%s %f\n", GREETING, a);
    printf("%s\n", STR(hello world));
#if DEBUG && !defined(NDEBUG)
    printf("debug on, m=%d\n", m);
#else
    printf("debug off\n");
#endif
#undef DEBUG
#ifdef DEBUG
    printf("never printed\n");
#endif
    return PIE;
}
