#include "comment_removal.h"
#include "strbuf.h"
#include <stdio.h>

char *remove_comments(const char *src)
{
    StrBuf out;
    sb_init(&out);
    size_t i = 0;

    while (src[i]) {
        char c = src[i];

        if (c == '"' || c == '\'') {                 /* string / char literal */
            char q = c;
            sb_putc(&out, src[i++]);
            while (src[i] && src[i] != q && src[i] != '\n') {
                if (src[i] == '\\' && src[i + 1])
                    sb_putc(&out, src[i++]);
                sb_putc(&out, src[i++]);
            }
            if (src[i] == q)
                sb_putc(&out, src[i++]);
        } else if (c == '/' && src[i + 1] == '/') {  /* single-line comment */
            i += 2;
            while (src[i] && src[i] != '\n') {
                if (src[i] == '\\' && src[i + 1] == '\n')   /* continued comment */
                    i++;
                i++;
            }
        } else if (c == '/' && src[i + 1] == '*') {  /* multi-line comment */
            i += 2;
            while (src[i] && !(src[i] == '*' && src[i + 1] == '/')) {
                if (src[i] == '\n')
                    sb_putc(&out, '\n');             /* keep line structure */
                i++;
            }
            if (src[i])
                i += 2;
            else
                fprintf(stderr, "mypreprocessor: warning: unterminated /* comment\n");
            sb_putc(&out, ' ');
        } else {
            sb_putc(&out, c);
            i++;
        }
    }
    return sb_detach(&out);
}
