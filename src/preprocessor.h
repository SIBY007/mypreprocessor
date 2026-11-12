#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include "strbuf.h"

/* Preprocesses a C source file starting at path.
 * Performs comment removal, file inclusion, conditional evaluation, and macro substitution.
 * Appends output to the given StrBuf.
 * Returns the number of errors encountered during preprocessing.
 */
int preprocess_file(const char *path, StrBuf *out, int depth);

#endif /* PREPROCESSOR_H */
