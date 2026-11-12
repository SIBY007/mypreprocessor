#include "macro_handler.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ACTIVE 64

typedef struct {
    char  *name;
    char **params;
    int    nparams;
    int    is_func;
    char  *body;
} Macro;

static Macro *table;
static size_t count, cap;

static int is_id_start(int c) { return isalpha(c) || c == '_'; }
static int is_id_char(int c)  { return isalnum(c) || c == '_'; }

static Macro *find(const char *name, size_t len)
{
    for (size_t i = 0; i < count; i++)
        if (strncmp(table[i].name, name, len) == 0 && table[i].name[len] == '\0')
            return &table[i];
    return NULL;
}

static void free_macro(Macro *m)
{
    free(m->name);
    free(m->body);
    for (int i = 0; i < m->nparams; i++)
        free(m->params[i]);
    free(m->params);
}

/* ---------------------------------------------------------------- define */

int macro_define(const char *p)
{
    Macro m;
    memset(&m, 0, sizeof m);

    while (isspace((unsigned char)*p)) p++;
    if (!is_id_start((unsigned char)*p)) {
        fprintf(stderr, "mypreprocessor: error: macro name missing in #define\n");
        return -1;
    }
    const char *s = p;
    while (is_id_char((unsigned char)*p)) p++;
    m.name = str_ndup(s, (size_t)(p - s));

    if (*p == '(') {                               /* function-like macro */
        m.is_func = 1;
        p++;
        for (;;) {
            while (isspace((unsigned char)*p)) p++;
            if (*p == ')') { p++; break; }
            if (!is_id_start((unsigned char)*p)) {
                fprintf(stderr, "mypreprocessor: error: bad parameter list in #define %s\n", m.name);
                free_macro(&m);
                return -1;
            }
            s = p;
            while (is_id_char((unsigned char)*p)) p++;
            m.params = realloc(m.params, sizeof(char *) * (size_t)(m.nparams + 1));
            m.params[m.nparams++] = str_ndup(s, (size_t)(p - s));
            while (isspace((unsigned char)*p)) p++;
            if (*p == ',') p++;
            else if (*p == ')') { p++; break; }
            else {
                fprintf(stderr, "mypreprocessor: error: bad parameter list in #define %s\n", m.name);
                free_macro(&m);
                return -1;
            }
        }
    }

    while (isspace((unsigned char)*p)) p++;
    size_t blen = strlen(p);
    while (blen > 0 && isspace((unsigned char)p[blen - 1])) blen--;
    m.body = str_ndup(p, blen);

    Macro *old = find(m.name, strlen(m.name));
    if (old) {                                     /* redefinition */
        free_macro(old);
        *old = m;
    } else {
        if (count == cap) {
            cap = cap ? cap * 2 : 32;
            table = realloc(table, cap * sizeof(Macro));
            if (!table) { perror("realloc"); exit(1); }
        }
        table[count++] = m;
    }
    return 0;
}

void macro_undef(const char *p)
{
    while (isspace((unsigned char)*p)) p++;
    const char *s = p;
    while (is_id_char((unsigned char)*p)) p++;
    Macro *m = find(s, (size_t)(p - s));
    if (!m) return;
    free_macro(m);
    *m = table[--count];
}

int macro_is_defined(const char *name) { return find(name, strlen(name)) != NULL; }

void macro_free_all(void)
{
    for (size_t i = 0; i < count; i++)
        free_macro(&table[i]);
    free(table);
    table = NULL;
    count = cap = 0;
}

/* ------------------------------------------------------------- expansion */

static void expand(const char *s, StrBuf *out, const char **active, int nactive);

static int is_active(const char **active, int n, const char *name)
{
    for (int i = 0; i < n; i++)
        if (strcmp(active[i], name) == 0)
            return 1;
    return 0;
}

/* Copy a string/char literal starting at s[*i]. */
static void copy_literal(const char *s, size_t *i, StrBuf *out)
{
    char q = s[*i];
    sb_putc(out, s[(*i)++]);
    while (s[*i] && s[*i] != q) {
        if (s[*i] == '\\' && s[*i + 1])
            sb_putc(out, s[(*i)++]);
        sb_putc(out, s[(*i)++]);
    }
    if (s[*i]) sb_putc(out, s[(*i)++]);
}

/*
 * s[*pos] must be '('. Splits the balanced argument list on top-level commas.
 * On success *pos is moved past ')' and 0 is returned.
 */
static int collect_args(const char *s, size_t *pos, char ***args, int *nargs)
{
    size_t i = *pos, start = i + 1;
    int depth = 0, n = 0;
    char **list = NULL;

    for (; s[i]; i++) {
        char c = s[i];
        if (c == '"' || c == '\'') {
            char q = c;
            for (i++; s[i] && s[i] != q; i++)
                if (s[i] == '\\' && s[i + 1]) i++;
            if (!s[i]) break;
        } else if (c == '(') {
            depth++;
        } else if ((c == ')' && depth == 1) || (c == ',' && depth == 1)) {
            size_t a = start, b = i;
            while (a < b && isspace((unsigned char)s[a])) a++;
            while (b > a && isspace((unsigned char)s[b - 1])) b--;
            list = realloc(list, sizeof(char *) * (size_t)(n + 1));
            list[n++] = str_ndup(s + a, b - a);
            start = i + 1;
            if (c == ')') {
                *pos = i + 1;
                *args = list;
                *nargs = n;
                return 0;
            }
        } else if (c == ')') {
            depth--;
        }
    }
    for (int k = 0; k < n; k++) free(list[k]);
    free(list);
    return -1;
}

static int param_index(const Macro *m, const char *s, size_t len)
{
    for (int i = 0; i < m->nparams; i++)
        if (strncmp(m->params[i], s, len) == 0 && m->params[i][len] == '\0')
            return i;
    return -1;
}

/* Substitute arguments into a function-like macro body (handles # and ##). */
static void substitute(const Macro *m, char **raw, char **expanded, StrBuf *out)
{
    const char *b = m->body;
    size_t i = 0;
    int paste_next = 0;

    while (b[i]) {
        char c = b[i];
        if (c == '"' || c == '\'') {
            copy_literal(b, &i, out);
            paste_next = 0;
        } else if (c == '#' && b[i + 1] == '#') {            /* token paste */
            sb_trim_trailing(out);
            i += 2;
            while (isspace((unsigned char)b[i])) i++;
            paste_next = 1;
        } else if (c == '#') {                               /* stringify */
            size_t j = i + 1;
            while (isspace((unsigned char)b[j])) j++;
            size_t k = j;
            while (is_id_char((unsigned char)b[k])) k++;
            int idx = (k > j && is_id_start((unsigned char)b[j])) ? param_index(m, b + j, k - j) : -1;
            if (idx >= 0) {
                sb_putc(out, '"');
                for (const char *a = raw[idx]; *a; a++) {
                    if (*a == '"' || *a == '\\') sb_putc(out, '\\');
                    sb_putc(out, *a);
                }
                sb_putc(out, '"');
                i = k;
            } else {
                sb_putc(out, c);
                i++;
            }
            paste_next = 0;
        } else if (isdigit((unsigned char)c)) {
            while (is_id_char((unsigned char)b[i]) || b[i] == '.')
                sb_putc(out, b[i++]);
            paste_next = 0;
        } else if (is_id_start((unsigned char)c)) {
            size_t j = i;
            while (is_id_char((unsigned char)b[j])) j++;
            int idx = param_index(m, b + i, j - i);
            if (idx >= 0) {
                size_t k = j;
                while (isspace((unsigned char)b[k])) k++;
                int next_paste = (b[k] == '#' && b[k + 1] == '#');
                sb_append(out, (paste_next || next_paste) ? raw[idx] : expanded[idx]);
            } else {
                sb_append_n(out, b + i, j - i);
            }
            i = j;
            paste_next = 0;
        } else {
            sb_putc(out, c);
            if (!isspace((unsigned char)c)) paste_next = 0;
            i++;
        }
    }
}

static void expand(const char *s, StrBuf *out, const char **active, int nactive)
{
    size_t i = 0;

    while (s[i]) {
        char c = s[i];

        if (c == '"' || c == '\'') {
            copy_literal(s, &i, out);
            continue;
        }
        if (isdigit((unsigned char)c)) {                     /* number: not an identifier */
            while (is_id_char((unsigned char)s[i]) || s[i] == '.')
                sb_putc(out, s[i++]);
            continue;
        }
        if (!is_id_start((unsigned char)c)) {
            sb_putc(out, c);
            i++;
            continue;
        }

        size_t j = i;
        while (is_id_char((unsigned char)s[j])) j++;
        Macro *m = find(s + i, j - i);

        if (!m || nactive >= MAX_ACTIVE || is_active(active, nactive, m->name)) {
            sb_append_n(out, s + i, j - i);
            i = j;
            continue;
        }

        const char *act2[MAX_ACTIVE];
        for (int k = 0; k < nactive; k++) act2[k] = active[k];
        act2[nactive] = m->name;

        if (!m->is_func) {                                   /* simple macro */
            StrBuf t;
            sb_init(&t);
            expand(m->body, &t, act2, nactive + 1);
            sb_append(out, t.data);
            sb_free(&t);
            i = j;
            continue;
        }

        /* function-like: needs '(' after optional whitespace */
        size_t k = j;
        while (isspace((unsigned char)s[k])) k++;
        if (s[k] != '(') {
            sb_append_n(out, s + i, j - i);
            i = j;
            continue;
        }

        char **args = NULL;
        int nargs = 0;
        size_t pos = k;
        if (collect_args(s, &pos, &args, &nargs) != 0) {
            fprintf(stderr, "mypreprocessor: warning: unterminated argument list for macro %s\n", m->name);
            sb_append_n(out, s + i, j - i);
            i = j;
            continue;
        }
        if (m->nparams == 0 && nargs == 1 && args[0][0] == '\0') {
            free(args[0]);
            nargs = 0;
        }
        if (nargs != m->nparams) {
            fprintf(stderr, "mypreprocessor: warning: macro %s expects %d argument(s), got %d\n",
                    m->name, m->nparams, nargs);
            for (int a = 0; a < nargs; a++) free(args[a]);
            free(args);
            sb_append_n(out, s + i, j - i);
            i = j;
            continue;
        }

        char **expanded = malloc(sizeof(char *) * (size_t)(nargs ? nargs : 1));
        for (int a = 0; a < nargs; a++) {
            StrBuf t;
            sb_init(&t);
            expand(args[a], &t, active, nactive);            /* args fully expanded first */
            expanded[a] = sb_detach(&t);
        }

        StrBuf sub, res;
        sb_init(&sub);
        sb_init(&res);
        substitute(m, args, expanded, &sub);
        expand(sub.data, &res, act2, nactive + 1);           /* rescan result */
        sb_append(out, res.data);

        sb_free(&sub);
        sb_free(&res);
        for (int a = 0; a < nargs; a++) { free(args[a]); free(expanded[a]); }
        free(args);
        free(expanded);
        i = pos;
    }
}

void macro_expand_line(const char *line, StrBuf *out)
{
    expand(line, out, NULL, 0);
}
