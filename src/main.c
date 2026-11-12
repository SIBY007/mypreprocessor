#include "file_inclusion.h"
#include "macro_handler.h"
#include "preprocessor.h"
#include "strbuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [-I dir]... inputfile.c\n"
                    "Writes the preprocessed result to inputfile.i\n", prog);
}

/* foo.c -> foo.i, dir/foo -> dir/foo.i */
static char *make_output_name(const char *in)
{
    const char *slash = strrchr(in, '/');
    const char *dot = strrchr(in, '.');
    size_t base = (dot && (!slash || dot > slash)) ? (size_t)(dot - in) : strlen(in);
    char *out = malloc(base + 3);
    if (!out) { perror("malloc"); exit(1); }
    memcpy(out, in, base);
    strcpy(out + base, ".i");
    return out;
}

int main(int argc, char **argv)
{
    const char *input = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-I") && i + 1 < argc)        add_include_dir(argv[++i]);
        else if (!strncmp(argv[i], "-I", 2) && argv[i][2]) add_include_dir(argv[i] + 2);
        else if (argv[i][0] == '-')                        { usage(argv[0]); return 2; }
        else if (!input)                                   input = argv[i];
        else                                               { usage(argv[0]); return 2; }
    }
    if (!input) { usage(argv[0]); return 2; }

    StrBuf out;
    sb_init(&out);
    int errors = preprocess_file(input, &out, 0);

    char *outname = make_output_name(input);
    FILE *f = fopen(outname, "wb");
    if (!f) {
        fprintf(stderr, "mypreprocessor: error: cannot write '%s'\n", outname);
        errors++;
    } else {
        fwrite(out.data, 1, out.len, f);
        fclose(f);
        if (errors == 0)
            printf("Created %s\n", outname);
        else
            fprintf(stderr, "Created %s with %d error(s)\n", outname, errors);
    }

    free(outname);
    sb_free(&out);
    macro_free_all();
    file_inclusion_cleanup();
    return errors ? 1 : 0;
}
