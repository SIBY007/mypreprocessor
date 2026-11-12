#ifndef CONDITIONAL_H
#define CONDITIONAL_H

/* Conditional compilation stack management (#if, #ifdef, #ifndef, #elif, #else, #endif) */
int  cond_active(void);
int  cond_depth(void);
int  cond_push(int v);
int  cond_wants_elif_eval(void);
int  cond_elif(int v);
int  cond_else(void);
int  cond_pop(void);

/* Evaluates an integer constant expression for #if and #elif directives.
 * Supports operators: !, -, +, ~, *, /, %, <, <=, >, >=, ==, !=, &&, ||, ?: and defined().
 * Sets *error to 1 if a syntax/evaluation error occurs.
 */
long cond_eval(const char *expr, int *error);

#endif /* CONDITIONAL_H */
