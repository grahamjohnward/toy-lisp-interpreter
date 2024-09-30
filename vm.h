#ifndef VM_H
#define VM_H
#include "lisp.h"

#define LISP_VM_STACK_SIZE 1024

struct vm_call_stack_frame {
    lisp_object_t code_vector;
    lisp_object_t instruction_pointer;
    /* This points to top of stack on entry to a function */
    lisp_object_t *frame_pointer;
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
};

void vm_init(struct vm *vm, size_t size);

void vm_free(struct vm *vm);

void init_vm_instruction_definitions();

void vm_print_stack(struct vm *vm);

lisp_object_t vm_pop(struct vm *vm);

lisp_object_t vm_peek(struct vm *vm);

void vm_run(struct vm *);

/** Instructions **/

void vm_inst_push(struct vm *vm, lisp_object_t obj);

void vm_inst_copy(struct vm *vm, lisp_object_t offset);

void vm_inst_copy2(struct vm *vm, lisp_object_t offset);

void vm_inst_swap_pop(struct vm *vm, lisp_object_t n);

void vm_inst_swap(struct vm *vm);

void vm_inst_pop(struct vm *vm);

void vm_inst_call(struct vm *vm, lisp_object_t n);

void vm_inst_ret(struct vm *vm);

void vm_inst_abort(struct vm *vm);

void vm_inst_jmp(struct vm *vm, lisp_object_t dest);

void vm_inst_jmp_if_nil(struct vm *vm, lisp_object_t dest);

#endif
