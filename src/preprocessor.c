#include "preprocessor.h"
#include "comment_removal.h"
#include "conditional.h"
#include "file_inclusion.h"
#include "macro_handler.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INCLUDE_DEPTH 32

static int is_directive(const char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    return *s == '#';
}

/* Handle one (already joined) directive line. Returns number of errors. */
static int handle_directive(const char *line, const char *path, const char *dir,
                            StrBuf *out, int depth)
{
    const char *p = line;
    int errors = 0;

    while (isspace((unsigned char)*p)) p++;
    p++;                                              /* skip '#' */
    while (*p == ' ' || *p == '\t') p++;

    const char *ns = p;
    while (isalpha((unsigned char)*p)) p++;
    size_t nlen = (size_t)(p - ns);
    char name[16] = "";
    if (nlen < sizeof name) { memcpy(name, ns, nlen); name[nlen] = '\0'; }
    const char *rest = p;
    while (isspace((unsigned char)*rest)) rest++;

    /* ---- conditionals are processed even in inactive regions ---- */
    if (!strcmp(name, "ifdef") || !strcmp(name, "ifndef")) {
        const char *s = rest;
        while (isalnum((unsigned char)*p) || *p == '_') p++;
        while (isalnum((unsigned char)*s) || *s == '_') s++;
        char *id = str_ndup(rest, (size_t)(s - rest));
        int def = macro_is_defined(id);
        free(id);
        if (cond_push(cond_active() && (name[2] == 'd' ? def : !def)) < 0) {
            fprintf(stderr, "%s: error: #if nested too deeply\n", path);
            errors++;
        }
        return errors;
    }
    if (!strcmp(name, "if")) {
        int e = 0;
        long v = cond_active() ? cond_eval(rest, &e) : 0;
        if (e) { fprintf(stderr, "%s: error: bad #if expression\n", path); errors++; }
        if (cond_push(v != 0) < 0) { fprintf(stderr, "%s: error: #if nested too deeply\n", path); errors++; }
        return errors;
    }
    if (!strcmp(name, "elif")) {
        int e = 0;
        long v = cond_wants_elif_eval() ? cond_eval(rest, &e) : 0;
        if (e) { fprintf(stderr, "%s: error: bad #elif expression\n", path); errors++; }
        if (cond_elif(v != 0) < 0) { fprintf(stderr, "%s: error: #elif without #if\n", path); errors++; }
        return errors;
    }
    if (!strcmp(name, "else")) {
        if (cond_else() < 0) { fprintf(stderr, "%s: error: #else without #if\n", path); errors++; }
        return errors;
    }
    if (!strcmp(name, "endif")) {
        if (cond_pop() < 0) { fprintf(stderr, "%s: error: #endif without #if\n", path); errors++; }
        return errors;
    }

    if (!cond_active())
        return 0;

    /* ---- the directives that matter when the region is active ---- */
    if (!strcmp(name, "define")) {
        if (macro_define(rest) < 0) errors++;
    } else if (!strcmp(name, "undef")) {
        macro_undef(rest);
    } else if (!strcmp(name, "include")) {
        char fname[1024];
        int sys = 0;
        if (parse_include(rest, fname, sizeof fname, &sys) < 0) {
            StrBuf ex;                                /* computed include: #include MACRO */
            sb_init(&ex);
            macro_expand_line(rest, &ex);
            int bad = parse_include(ex.data, fname, sizeof fname, &sys);
            sb_free(&ex);
            if (bad) {
                fprintf(stderr, "%s: error: malformed #include\n", path);
                return 1;
            }
        }
        char *found = resolve_include(fname, sys, dir);
        if (!found) {
            fprintf(stderr, "%s: %s: cannot open include file '%s'%s\n", path,
                    sys ? "warning" : "error", fname, sys ? " (left as is)" : "");
            if (!sys) errors++;
            sb_append(out, line);                     /* keep the line untouched */
            sb_putc(out, '\n');
        } else {
            if (!once_seen(found))
                errors += preprocess_file(found, out, depth + 1);
            free(found);
        }
    } else if (!strcmp(name, "error")) {
        fprintf(stderr, "%s: #error %s\n", path, rest);
        errors++;
    } else if (!strcmp(name, "warning")) {
        fprintf(stderr, "%s: warning: %s\n", path, rest);
    } else if (!strcmp(name, "pragma") && !strncmp(rest, "once", 4)) {
        once_mark(path);
    } else if (*name) {                               /* #pragma, #line, ... pass through */
        sb_append(out, line);
        sb_putc(out, '\n');
    }
    return errors;
}

static int process_text(const char *text, const char *path, StrBuf *out, int depth)
{
    char *dir = dir_of(path);
    int errors = 0;
    int base_depth = cond_depth();
    const char *p = text;

    while (*p) {
        StrBuf line;
        sb_init(&line);

        for (;;) {                                    /* read one logical line */
            const char *eol = strchr(p, '\n');
            size_t len = eol ? (size_t)(eol - p) : strlen(p);
            if (len > 0 && p[len - 1] == '\r') len--;
            sb_append_n(&line, p, len);
            p += eol ? (size_t)(eol - p) + 1 : len;
            if (!eol && *p) p += strlen(p);
            /* join backslash-continued lines, but only for directives */
            if (is_directive(line.data) && line.len > 0 && line.data[line.len - 1] == '\\' && *p) {
                line.data[--line.len] = '\0';
                sb_putc(&line, ' ');
                continue;
            }
            break;
        }

        if (is_directive(line.data)) {
            errors += handle_directive(line.data, path, dir, out, depth);
        } else if (cond_active()) {
            StrBuf ex;
            sb_init(&ex);
            macro_expand_line(line.data, &ex);
            sb_trim_trailing(&ex);
            sb_append(out, ex.data);
            sb_putc(out, '\n');
            sb_free(&ex);
        }
        sb_free(&line);
    }

    if (cond_depth() > base_depth) {
        fprintf(stderr, "%s: error: unterminated #if / #ifdef\n", path);
        errors++;
        while (cond_depth() > base_depth) cond_pop();
    }
    free(dir);
    return errors;
}

int preprocess_file(const char *path, StrBuf *out, int depth)
{
    if (depth > MAX_INCLUDE_DEPTH) {
        fprintf(stderr, "%s: error: #include nested too deeply\n", path);
        return 1;
    }
    char *raw = read_file(path);
    if (!raw) {
        fprintf(stderr, "mypreprocessor: error: cannot open '%s'\n", path);
        return 1;
    }
    char *clean = remove_comments(raw);               /* step 1: comments */
    free(raw);
    int errors = process_text(clean, path, out, depth); /* steps 2+3: include & macros */
    free(clean);
    return errors;
}
