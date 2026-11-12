# mypreprocessor

A small C preprocessor written in C. It reads a C source file and writes an
expanded file with the same base name and a `.i` extension.

```
./mypreprocessor inputfile.c      # creates inputfile.i
```

It performs three jobs, in this order:

1. **Comment removal** – `// ...` and `/* ... */` are removed. Comment markers
   inside string or character literals are left alone, and newlines inside a
   block comment are kept so the code layout is preserved.
2. **File inclusion** – `#include <file>` / `#include "file"` is replaced by the
   (recursively processed) contents of the file.
3. **Macro substitution** – `#define` stores a macro; every later use is replaced.
   Both forms are supported:
   * simple: `#define PI 3.14`
   * with arguments: `#define SQUARE(x) ((x)*(x))`

Bonus features: `#undef`, `#if / #ifdef / #ifndef / #elif / #else / #endif`
(with `defined()` and integer expressions), `#pragma once`, `#` stringify and
`##` token pasting, `#error`, multi-line (`\`-continued) `#define`s, `-I dir`.

> This is a software-only project, so there is no circuit / hardware section.

## Build and run

```
make          # builds ./mypreprocessor
make test     # runs tests/run_tests.sh
make clean
./mypreprocessor tests/sample.c && cat tests/sample.i
```

Requires `gcc` and `make` (Linux / macOS / WSL).

## Project layout (modular)

| File | Responsibility |
|------|----------------|
| `src/main.c` | Command-line handling, output file name (`.c` → `.i`) |
| `src/preprocessor.c/.h` | Driver: reads logical lines, dispatches directives |
| `src/comment_removal.c/.h` | Comment stripping state machine |
| `src/file_inclusion.c/.h` | Reading files, parsing/locating `#include`, `#pragma once` |
| `src/macro_handler.c/.h` | Macro table, `#define`/`#undef`, argument substitution, expansion |
| `src/conditional.c/.h` | `#if` family stack and expression evaluator |
| `src/strbuf.c/.h` | Dynamic string buffer helper |
| `Makefile` | Builds all modules into `mypreprocessor` |
| `tests/` | `sample.c`, `mylib.h`, expected output, `run_tests.sh` |

## How it works

```
input.c ──► remove_comments ──► line loop ──┬─ #include → recurse into file (same steps)
                                            ├─ #define  → store in macro table
                                            ├─ #if...   → conditional stack
                                            └─ text     → macro_expand_line → output
                                                                   │
                                                              input.i
```

**Macro expansion** scans each line token by token. String/char literals and
numbers are skipped, so `"PI"` and `0x1F` are never touched and `PIE` is not
confused with `PI`. For a function-like macro the argument list is read with
balanced parentheses (so `F((1,2), 3)` has two arguments), arguments are
expanded, substituted into the body, and the result is rescanned. A macro that
is currently being expanded is not expanded again, so `#define A B` /
`#define B A` terminates instead of looping forever.

**Include search order:** for `"file"` – the including file's directory, the
current directory, then `-I` directories; for `<file>` – `-I` directories, then
the same local places. If a `<system>` header is not found (e.g. `<stdio.h>`) the
line is left in the output with a warning; a missing `"file"` is an error.
Include depth is limited to 32.

## Example

Input (`tests/sample.c`, shortened):

```c
#include <mylib.h>              // defines PI and SQUARE(x)
#define AREA(r) (PI * SQUARE(r))
double a = AREA(r);             /* comment */
```

Output (`sample.i`):

```c
double a = (3.14 * ((r)*(r)));
```

## Known limitations

* Not a full replacement for `cpp`: no variadic macros (`...`), no `__FILE__`/`__LINE__`,
  no linemarkers, no trigraphs.
* System headers are not searched (e.g. `/usr/include`).
* `#if` supports integers, `defined`, `! - + ~ * / % < > <= >= == != && || ?:` only.
