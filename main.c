#include "lisp.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    int i = 1;
    size_t heap_size = 65536; /* Default */
    if (strcmp(argv[i], "--heap-size") == 0) {
        i++;
        char *endptr;
        errno = 0;
        heap_size = strtol(argv[i], &endptr, 10);
        if (errno) {
            perror("Heap size");
            exit(1);
        } else if (*endptr != '\0') {
            printf("Bad heap size %s\n", argv[i]);
            exit(1);
        } else {
            i++;
        }
    }
    init_interpreter(heap_size);
    top_of_stack = (lisp_object_t *)get_rbp(1);
    for (; i < argc; i++) {
        load_str(argv[i]);
    }
    free_interpreter();
    return 0;
}
