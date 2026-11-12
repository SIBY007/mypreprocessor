#ifndef FILE_INCLUSION_H
#define FILE_INCLUSION_H

#include <stddef.h>

/* Reads an entire file into a dynamically allocated, null-terminated buffer. */
char *read_file(const char *path);

/* Returns the directory portion of a file path (dynamically allocated). */
char *dir_of(const char *path);

/* Appends an include directory to the search path (-I flag). */
void add_include_dir(const char *dir);

/* Parses #include <file> or #include "file".
 * Sets *is_system to 1 for <...>, 0 for "...". Returns 0 on success, -1 on failure.
 */
int parse_include(const char *p, char *name, size_t n, int *is_system);

/* Resolves an include filename according to include search rules:
 * - For "file": current_dir, then ".", then -I directories.
 * - For <file>: -I directories, then current_dir, then ".".
 * Returns a dynamically allocated path string, or NULL if not found.
 */
char *resolve_include(const char *name, int is_system, const char *current_dir);

/* Tracks #pragma once file inclusions to prevent circular / duplicate includes. */
void once_mark(const char *path);
int  once_seen(const char *path);

/* Frees stored include directories and seen #pragma once paths. */
void file_inclusion_cleanup(void);

#endif /* FILE_INCLUSION_H */
