#ifndef VM_H
#define VM_H
#include "lisp.h"

#include <setjmp.h>

#define LISP_VM_STACK_SIZE 1024

struct vm_call_stack_frame {
    lisp_object_t code_vector;
    lisp_object_t instruction_pointer;
    lisp_object_t environment;
    lisp_object_t tags;
};

struct vm {
    /* Data stack */
    size_t size;
    lisp_object_t *data_stack;
    lisp_object_t *top_of_data_stack;
    /* Call stack */
    struct vm_call_stack_frame *call_stack;
    struct vm_call_stack_frame *call_stack_pointer;
    /* Registers */
    struct vm_call_stack_frame registers;
    /* For throwing exceptions from built-in functions */
    jmp_buf jmp_buf;
    int vm_trace;
};

void vm_init(struct vm *vm, size_t size);

void vm_free(struct vm *vm);

void init_vm_instruction_definitions();

void vm_print_stack(struct vm *vm);

lisp_object_t vm_get_stack(struct vm *vm);

lisp_object_t vm_pop(struct vm *vm);

lisp_object_t vm_peek(struct vm *vm);

void vm_run(struct vm *);

/** Instructions **/

void vm_inst_push(struct vm *vm, lisp_object_t obj);

void vm_inst_pop(struct vm *vm);

void vm_inst_call(struct vm *vm);

void vm_inst_ret(struct vm *vm);

void vm_inst_get(struct vm *vm, lisp_object_t n, lisp_object_t m);

void vm_inst_set(struct vm *vm, lisp_object_t n, lisp_object_t m);

void vm_inst_abort(struct vm *vm);

void vm_inst_jmp(struct vm *vm, lisp_object_t dest);

void vm_inst_jmp_if_nil(struct vm *vm, lisp_object_t dest);

void vm_inst_set_tag(struct vm *vm, lisp_object_t tag, lisp_object_t dest);

void vm_inst_tag_jmp(struct vm *vm, lisp_object_t tag);

void vm_inst_raise(struct vm *vm);

void vm_inst_nop(struct vm *vm);

#endif
