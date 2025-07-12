#ifndef INTERP_H
#define INTERP_H

#include <setjmp.h>

#include "lisp.h"
#include "syms.h"
#include "vm.h"

struct return_context {
    lisp_object_t type;
    jmp_buf buf;
    lisp_object_t return_value;
    struct return_context *next;
    /* tagbody_forms is here so it can be freed in pop_return_context() */
    /* - it is not actually accessed: */
    lisp_object_t *tagbody_forms;
    size_t tagbody_forms_len;
};

struct lisp_interpreter {
    struct syms syms;
    lisp_object_t symbol_table;
    struct return_context *return_stack;
    struct lisp_heap heap;
    lisp_object_t *top_of_stack;
    struct vm vm;
    int use_vm;
};

extern struct lisp_interpreter *interp;

#endif
