#ifndef MACRO_HANDLER_H
#define MACRO_HANDLER_H

#include "strbuf.h"

/* Defines a macro from line text following #define.
 * Handles both object-like macros (#define PI 3.14) and
 * function-like macros with parameters (#define SQUARE(x) ((x)*(x))).
 * Returns 0 on success, -1 on syntax error.
 */
int macro_define(const char *p);

/* Removes a macro definition matching #undef <name>. */
void macro_undef(const char *p);

/* Checks if a macro name is currently defined. Returns 1 if defined, 0 otherwise. */
int macro_is_defined(const char *name);

/* Frees all allocated macro definitions in the macro table. */
void macro_free_all(void);

/* Expands all macros within a line and appends the expanded text to out. */
void macro_expand_line(const char *line, StrBuf *out);

#endif /* MACRO_HANDLER_H */
