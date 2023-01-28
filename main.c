#include "lisp.h"

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct interpreter_settings {
    size_t heap_size;
    char *image;
};

static struct option options[] = {
    { "heap-size", optional_argument, 0, 1 },
    { "image", optional_argument, 0, 2 },
    { 0, 0, 0, 0 }
};

static int parse_args(int argc, char **argv, struct interpreter_settings *settings)
{
    settings->heap_size = 65536; /* default */
    settings->image = NULL;
    int c;
    while (1) {
        int option_index;
        c = getopt_long_only(argc, argv, "", options, &option_index);
        if (c == -1)
            break;
        if (!optarg) {
            printf("%s: missing argument\n", options[option_index].name);
            exit(1);
        }
        switch (c) {
            char *endptr;
        case 1:
            errno = 0;
            settings->heap_size = strtol(optarg, &endptr, 10);
            if (errno) {
                perror("Heap size");
                exit(1);
            } else if (*endptr != '\0') {
                printf("Bad heap size %s\n", optarg);
                exit(1);
            }
            break;
        case 2:
            settings->image = malloc(strlen(optarg));
            strcpy(settings->image, optarg);
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
    if (settings.image)
        init_interpeter_from_image(settings.image);
    else
        init_interpreter(settings.heap_size);
    top_of_stack = (lisp_object_t *)get_rbp(1);
    for (; i < argc; i++)
        load_str(argv[i]);
    free_interpreter();
    if (settings.image)
        free(settings.image);
    return 0;
}
