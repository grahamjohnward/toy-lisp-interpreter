#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "lisp.h"

int main(int argc, char **argv)
{
    init_interpreter(32768);
    for (int i = 1; i < argc; i++) {
	load_str(argv[i]);
    }
    free_interpreter();
    return 0;
}
