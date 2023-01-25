#include "lisp.h"

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct interpreter_settings {
    size_t heap_size;
};

static struct option options[] = {
    { "heap-size", optional_argument, 0, 0 },
    { 0, 0, 0, 0 }
};

static int parse_args(int argc, char **argv, struct interpreter_settings *settings)
{
    settings->heap_size = 65536; /* default */
    int c;
    while (1) {
        c = getopt_long_only(argc, argv, "", options, NULL);
        printf("%d\n", c);
        if (c == -1)
            break;
        switch (c) {
        case 0:
            if (optarg) {
                char *endptr;
                errno = 0;
                settings->heap_size = strtol(optarg, &endptr, 10);
                if (errno) {
                    perror("Heap size");
                    exit(1);
                } else if (*endptr != '\0') {
                    printf("Bad heap size %s\n", optarg);
                    exit(1);
                }
            }
            break;
        default:
            abort();
        }
    }
    return optind;
}

int main(int argc, char **argv)
{
    struct interpreter_settings settings;
    int i = parse_args(argc, argv, &settings);
    init_interpreter(settings.heap_size);
    top_of_stack = (lisp_object_t *)get_rbp(1);
    for (; i < argc; i++)
        load_str(argv[i]);
    free_interpreter();
    return 0;
}
