#include "file_inclusion.h"
#include "strbuf.h"
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#if defined(_WIN32) && !defined(__CYGWIN__)
#define realpath(N, R) _fullpath((R), (N), PATH_MAX)
#endif

static char **inc_dirs;
static size_t n_inc;
static char **once_list;
static size_t n_once;

char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    StrBuf sb;
    sb_init(&sb);
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0)
        sb_append_n(&sb, buf, n);
    fclose(f);
    return sb_detach(&sb);
}

char *dir_of(const char *path)
{
    const char *slash = strrchr(path, '/');
    if (!slash) return str_dup(".");
    if (slash == path) return str_dup("/");
    return str_ndup(path, (size_t)(slash - path));
}

void add_include_dir(const char *dir)
{
    inc_dirs = realloc(inc_dirs, sizeof(char *) * (n_inc + 1));
    inc_dirs[n_inc++] = str_dup(dir);
}

int parse_include(const char *p, char *name, size_t n, int *is_system)
{
    while (isspace((unsigned char)*p)) p++;
    char close;
    if (*p == '<')      { close = '>'; *is_system = 1; }
    else if (*p == '"') { close = '"'; *is_system = 0; }
    else return -1;
    p++;
    const char *end = strchr(p, close);
    if (!end || (size_t)(end - p) == 0 || (size_t)(end - p) >= n) return -1;
    memcpy(name, p, (size_t)(end - p));
    name[end - p] = '\0';
    return 0;
}

static char *try_path(const char *dir, const char *name)
{
    char path[PATH_MAX];
    if (name[0] == '/' || !dir)
        snprintf(path, sizeof path, "%s", name);
    else
        snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fclose(f);
    return str_dup(path);
}

char *resolve_include(const char *name, int is_system, const char *current_dir)
{
    char *r;
    if (name[0] == '/')
        return try_path(NULL, name);

    if (!is_system) {                         /* "file": includer's dir first */
        if ((r = try_path(current_dir, name))) return r;
        if ((r = try_path(".", name)))         return r;
    }
    for (size_t i = 0; i < n_inc; i++)
        if ((r = try_path(inc_dirs[i], name))) return r;
    if (is_system) {                          /* <file>: also look locally */
        if ((r = try_path(current_dir, name))) return r;
        if ((r = try_path(".", name)))         return r;
    }
    return NULL;
}

void once_mark(const char *path)
{
    char rp[PATH_MAX];
    if (!realpath(path, rp)) return;
    once_list = realloc(once_list, sizeof(char *) * (n_once + 1));
    once_list[n_once++] = str_dup(rp);
}

int once_seen(const char *path)
{
    char rp[PATH_MAX];
    if (!realpath(path, rp)) return 0;
    for (size_t i = 0; i < n_once; i++)
        if (strcmp(once_list[i], rp) == 0) return 1;
    return 0;
}

void file_inclusion_cleanup(void)
{
    for (size_t i = 0; i < n_inc; i++) free(inc_dirs[i]);
    for (size_t i = 0; i < n_once; i++) free(once_list[i]);
    free(inc_dirs); free(once_list);
    inc_dirs = NULL; once_list = NULL;
    n_inc = n_once = 0;
}
