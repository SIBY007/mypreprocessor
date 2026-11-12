#include "strbuf.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void grow(StrBuf *sb, size_t need)
{
    if (sb->data && sb->len + need + 1 <= sb->cap)
        return;
    size_t nc = sb->cap ? sb->cap : 64;
    while (nc < sb->len + need + 1)
        nc *= 2;
    char *p = realloc(sb->data, nc);
    if (!p) {
        perror("mypreprocessor: out of memory");
        exit(1);
    }
    sb->data = p;
    sb->cap = nc;
}

void sb_init(StrBuf *sb)
{
    sb->data = NULL;
    sb->len = sb->cap = 0;
    grow(sb, 0);
    sb->data[0] = '\0';
}

void sb_append_n(StrBuf *sb, const char *s, size_t n)
{
    grow(sb, n);
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

void sb_append(StrBuf *sb, const char *s) { sb_append_n(sb, s, strlen(s)); }
void sb_putc(StrBuf *sb, char c)           { sb_append_n(sb, &c, 1); }

void sb_trim_trailing(StrBuf *sb)
{
    while (sb->len > 0 && isspace((unsigned char)sb->data[sb->len - 1]))
        sb->data[--sb->len] = '\0';
}

char *sb_detach(StrBuf *sb)
{
    char *p = sb->data;
    sb->data = NULL;
    sb->len = sb->cap = 0;
    return p;
}

void sb_free(StrBuf *sb)
{
    free(sb->data);
    sb->data = NULL;
    sb->len = sb->cap = 0;
}

char *str_ndup(const char *s, size_t n)
{
    char *p = malloc(n + 1);
    if (!p) {
        perror("mypreprocessor: out of memory");
        exit(1);
    }
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

char *str_dup(const char *s) { return str_ndup(s, strlen(s)); }
