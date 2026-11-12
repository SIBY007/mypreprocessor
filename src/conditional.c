#include "conditional.h"
#include "macro_handler.h"
#include "strbuf.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define MAX_COND 64

static struct { int parent, taken, cur, seen_else; } st[MAX_COND];
static int sp;

int cond_active(void) { return sp == 0 ? 1 : st[sp - 1].cur; }
int cond_depth(void)  { return sp; }

int cond_push(int v)
{
    if (sp >= MAX_COND) return -1;
    int parent = cond_active();
    st[sp].parent = parent;
    st[sp].cur = st[sp].taken = parent && v;
    st[sp].seen_else = 0;
    sp++;
    return 0;
}

int cond_wants_elif_eval(void)
{
    return sp > 0 && st[sp - 1].parent && !st[sp - 1].taken;
}

int cond_elif(int v)
{
    if (sp == 0 || st[sp - 1].seen_else) return -1;
    int i = sp - 1;
    if (st[i].taken) st[i].cur = 0;
    else { st[i].cur = st[i].parent && v; if (st[i].cur) st[i].taken = 1; }
    return 0;
}

int cond_else(void)
{
    if (sp == 0 || st[sp - 1].seen_else) return -1;
    int i = sp - 1;
    st[i].seen_else = 1;
    st[i].cur = st[i].parent && !st[i].taken;
    st[i].taken = 1;
    return 0;
}

int cond_pop(void)
{
    if (sp == 0) return -1;
    sp--;
    return 0;
}

/* ------------------------------------------------- #if expression parser */

static const char *p;
static int err;

static void ws(void) { while (*p && isspace((unsigned char)*p)) p++; }
static int eat(const char *s)
{
    ws();
    size_t n = strlen(s);
    if (strncmp(p, s, n) == 0) { p += n; return 1; }
    return 0;
}

static long e_top(void);

static long e_prim(void)
{
    ws();
    if (*p == '(') {
        p++;
        long v = e_top();
        if (!eat(")")) err = 1;
        return v;
    }
    if (isdigit((unsigned char)*p)) {
        char *e;
        long v = strtol(p, &e, 0);
        p = e;
        while (*p == 'u' || *p == 'U' || *p == 'l' || *p == 'L') p++;
        return v;
    }
    if (isalpha((unsigned char)*p) || *p == '_') {           /* unknown identifier => 0 */
        while (isalnum((unsigned char)*p) || *p == '_') p++;
        ws();
        if (*p == '(') {
            int d = 0;
            do { if (*p == '(') d++; else if (*p == ')') d--; p++; } while (*p && d > 0);
        }
        return 0;
    }
    if (*p == '\'') {
        long v;
        p++;
        if (*p == '\\') {
            p++;
            switch (*p) { case 'n': v = 10; break; case 't': v = 9; break; case '0': v = 0; break; default: v = *p; }
            p++;
        } else v = (unsigned char)*p++;
        if (*p == '\'') p++; else err = 1;
        return v;
    }
    err = 1;
    return 0;
}

static long e_unary(void)
{
    ws();
    if (*p == '!' && p[1] != '=') { p++; return !e_unary(); }
    if (*p == '-') { p++; return -e_unary(); }
    if (*p == '+') { p++; return e_unary(); }
    if (*p == '~') { p++; return ~e_unary(); }
    return e_prim();
}

static long e_mul(void)
{
    long v = e_unary();
    for (;;) {
        ws();
        if (*p == '*') { p++; v *= e_unary(); }
        else if (*p == '/') { p++; long d = e_unary(); if (!d) err = 1; else v /= d; }
        else if (*p == '%') { p++; long d = e_unary(); if (!d) err = 1; else v %= d; }
        else break;
    }
    return v;
}

static long e_add(void)
{
    long v = e_mul();
    for (;;) {
        ws();
        if (*p == '+') { p++; v += e_mul(); }
        else if (*p == '-') { p++; v -= e_mul(); }
        else break;
    }
    return v;
}

static long e_rel(void)
{
    long v = e_add();
    for (;;) {
        long r;
        if (eat("<="))      { r = e_add(); v = v <= r; }
        else if (eat(">=")) { r = e_add(); v = v >= r; }
        else {
            ws();
            if (*p == '<' && p[1] != '<')      { p++; r = e_add(); v = v < r; }
            else if (*p == '>' && p[1] != '>') { p++; r = e_add(); v = v > r; }
            else break;
        }
    }
    return v;
}

static long e_eq(void)
{
    long v = e_rel();
    for (;;) {
        long r;
        if (eat("=="))      { r = e_rel(); v = v == r; }
        else if (eat("!=")) { r = e_rel(); v = v != r; }
        else break;
    }
    return v;
}

static long e_land(void)
{
    long v = e_eq();
    while (eat("&&")) { long r = e_eq(); v = v && r; }
    return v;
}

static long e_lor(void)
{
    long v = e_land();
    while (eat("||")) { long r = e_land(); v = v || r; }
    return v;
}

static long e_top(void)
{
    long c = e_lor();
    if (eat("?")) {
        long a = e_top();
        if (!eat(":")) err = 1;
        long b = e_top();
        return c ? a : b;
    }
    return c;
}

static int id_start(int c) { return isalpha(c) || c == '_'; }
static int id_char(int c)  { return isalnum(c) || c == '_'; }

/* Replace  defined X  /  defined(X)  by 1 or 0 before macro expansion. */
static void replace_defined(const char *s, StrBuf *out)
{
    size_t i = 0;
    while (s[i]) {
        if (id_start((unsigned char)s[i]) && (i == 0 || !id_char((unsigned char)s[i - 1]))) {
            size_t j = i;
            while (id_char((unsigned char)s[j])) j++;
            if (j - i == 7 && strncmp(s + i, "defined", 7) == 0) {
                size_t k = j;
                while (isspace((unsigned char)s[k])) k++;
                int paren = (s[k] == '(');
                if (paren) { k++; while (isspace((unsigned char)s[k])) k++; }
                size_t a = k;
                while (id_char((unsigned char)s[k])) k++;
                char *name = str_ndup(s + a, k - a);
                sb_putc(out, macro_is_defined(name) ? '1' : '0');
                free(name);
                while (isspace((unsigned char)s[k])) k++;
                if (paren && s[k] == ')') k++;
                i = k;
            } else {
                sb_append_n(out, s + i, j - i);
                i = j;
            }
        } else {
            sb_putc(out, s[i++]);
        }
    }
}

long cond_eval(const char *expr, int *error)
{
    StrBuf a, b;
    sb_init(&a);
    sb_init(&b);
    replace_defined(expr, &a);
    macro_expand_line(a.data, &b);

    p = b.data;
    err = 0;
    long v = e_top();
    ws();
    if (*p) err = 1;
    if (error) *error = err;
    sb_free(&a);
    sb_free(&b);
    return err ? 0 : v;
}
