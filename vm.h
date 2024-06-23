#include "lisp.h"

#define LISP_VM_STACK_SIZE 1024

struct vm {
    size_t size;
    lisp_object_t *data_stack;
    lisp_object_t *top_of_data_stack;
    lisp_object_t current_code_vector;
    lisp_object_t call_stack;
    lisp_object_t instruction_pointer;
};

void vm_init(struct vm *vm, size_t size);

void vm_free(struct vm *vm);

void vm_print_stack(struct vm *vm);

lisp_object_t vm_pop2(struct vm *vm);

lisp_object_t vm_peek(struct vm *vm);

void vm_run(struct vm *);

void vm_run_instruction(struct vm *vm, lisp_object_t ins);

/** Instructions **/

void vm_inst_push(struct vm *vm, lisp_object_t obj);

void vm_inst_copy(struct vm *vm, lisp_object_t offset);

void vm_inst_swap(struct vm *vm);

void vm_inst_pop(struct vm *vm);

void vm_inst_call(struct vm *vm);

void vm_inst_ret(struct vm *vm);
