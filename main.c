#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "lisp.h"

int main(int argc, char **argv)
{
    init_interpreter(32768);
    top_of_stack = (lisp_object_t*)get_rbp(1);
    for (int i = 1; i < argc; i++) {
	load_str(argv[i]);
    }
    free_interpreter();
    return 0;
}
