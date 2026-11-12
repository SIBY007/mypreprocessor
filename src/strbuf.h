#ifndef STRBUF_H
#define STRBUF_H

#include <stddef.h>

typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} StrBuf;

void  sb_init(StrBuf *sb);
void  sb_append_n(StrBuf *sb, const char *s, size_t n);
void  sb_append(StrBuf *sb, const char *s);
void  sb_putc(StrBuf *sb, char c);
void  sb_trim_trailing(StrBuf *sb);
char *sb_detach(StrBuf *sb);
void  sb_free(StrBuf *sb);

char *str_ndup(const char *s, size_t n);
char *str_dup(const char *s);

#endif /* STRBUF_H */
