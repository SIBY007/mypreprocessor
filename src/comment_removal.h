#ifndef COMMENT_REMOVAL_H
#define COMMENT_REMOVAL_H

/* Removes single-line (//) and multi-line (/* ... * /) comments from C source.
 * Preserves newlines in multi-line comments to maintain original line numbering.
 * Preserves comment delimiters occurring within string and character literals.
 * Returns a dynamically allocated string that the caller must free.
 */
char *remove_comments(const char *src);

#endif /* COMMENT_REMOVAL_H */
