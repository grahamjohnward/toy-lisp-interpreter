#include "vm.h"

#include <assert.h>
#include <stdio.h>

void vm_init(struct vm *vm, size_t size)
{
    vm->size = size;
    vm->data_stack = (lisp_object_t *)malloc(sizeof(lisp_object_t) * vm->size);
    vm->top_of_data_stack = vm->data_stack;
    vm->current_code_vector = NIL;
    vm->call_stack = NIL;
    vm->instruction_pointer = 0;
}

void vm_free(struct vm *vm)
{
    free(vm->data_stack);
}

void vm_print_stack(struct vm *vm)
{
    /* Visually speaking, stack grows up here: */
    printf("STACK ->\n");
    for (lisp_object_t *p = vm->top_of_data_stack - 1; p >= vm->data_stack; p--) {
        lisp_object_t obj = *p;
        char *str = print_object(obj);
        printf("%s\n", str);
        free(str);
    }
    printf("<- STACK\n");
}

lisp_object_t vm_pop2(struct vm *vm)
{
    assert(vm->top_of_data_stack >= vm->data_stack);
    return *(--vm->top_of_data_stack);
}

lisp_object_t vm_peek(struct vm *vm)
{
    assert(vm->top_of_data_stack > vm->data_stack);
    return *(vm->top_of_data_stack - 1);
}

void vm_run(struct vm *vm)
{
    while (vm->instruction_pointer < length(vm->current_code_vector)) {
        lisp_object_t instruction = svref(vm->current_code_vector, vm->instruction_pointer);
        vm->instruction_pointer += 16; /* Since it's a lisp integer */
        vm_run_instruction(vm, instruction);
    }
}

void vm_run_instruction(struct vm *vm, lisp_object_t ins)
{
    TRACE(vm->call_stack);
    vm_print_stack(vm);
    TRACE(vm->instruction_pointer);
    TRACE(ins);
    if (consp(ins) != NIL) {
        lisp_object_t first = car(ins);
        if (first == sym("push")) {
            vm_inst_push(vm, cadr(ins));
        } else if (first == sym("copy")) {
            vm_inst_copy(vm, cadr(ins));
        } else {
            abort();
        }
    } else if (ins == sym("swap")) {
        vm_inst_swap(vm);
    } else if (ins == sym("pop")) {
        vm_inst_pop(vm);
    } else if (ins == sym("call")) {
        vm_inst_call(vm);
    } else if (ins == sym("ret")) {
        vm_inst_ret(vm);
    } else {
        abort();
    }
}

/** Instructions **/

void vm_inst_push(struct vm *vm, lisp_object_t obj)
{
    assert(vm->top_of_data_stack - vm->data_stack < vm->size);
    *(vm->top_of_data_stack++) = obj;
}

void vm_inst_copy(struct vm *vm, lisp_object_t offset)
{
    vm_inst_push(vm, *(vm->top_of_data_stack - (offset >> 4) - 1));
}

void vm_inst_swap(struct vm *vm)
{
    lisp_object_t top = *(vm->top_of_data_stack - 1);
    lisp_object_t second = *(vm->top_of_data_stack - 2);
    *(vm->top_of_data_stack - 1) = second;
    *(vm->top_of_data_stack - 2) = top;
}

void vm_inst_pop(struct vm *vm)
{
    vm->top_of_data_stack--;
}

void vm_inst_call(struct vm *vm)
{
    lisp_object_t fn = vm_pop2(vm);
    struct lisp_function *fnptr = LispFunctionPtr(fn);
    lisp_object_t result = NIL;
    if (fnptr->kind == interp->syms.built_in_function) {
        lisp_object_t actual_function = fnptr->actual_function;
        void (*fp)() = FunctionPtr(cadr(actual_function));
        int arity = ((int64_t)caddr(actual_function)) >> 4;
        lisp_object_t arg1 = NIL;
        lisp_object_t arg2 = NIL;
        lisp_object_t arg3 = NIL;
        switch (arity) {
        case 0:
            result = ((lisp_object_t(*)())fp)();
            break;
        case 1:
            arg1 = vm_pop2(vm);
            result = ((lisp_object_t(*)(lisp_object_t))fp)(arg1);
            break;
        case 2:
            arg2 = vm_pop2(vm);
            arg1 = vm_pop2(vm);
            result = ((lisp_object_t(*)(lisp_object_t, lisp_object_t))fp)(arg1, arg2);
            break;
        case 3:
            arg3 = vm_pop2(vm);
            arg2 = vm_pop2(vm);
            arg1 = vm_pop2(vm);
            result = ((lisp_object_t(*)(lisp_object_t, lisp_object_t, lisp_object_t))fp)(arg1, arg2, arg3);
            break;
        default:
            abort();
        }
        vm_inst_push(vm, result);
    } else if (fnptr->kind == interp->syms.lambda) {
        vm->call_stack = cons(cons(vm->instruction_pointer, vm->current_code_vector), vm->call_stack);
        vm->current_code_vector = fnptr->actual_function;
        vm->instruction_pointer = 0;
    } else {
        abort();
    }
}

void vm_inst_ret(struct vm *vm)
{
    assert(vm->call_stack != NIL);
    lisp_object_t thing = car(vm->call_stack);
    vm->instruction_pointer = car(thing);
    vm->current_code_vector = cdr(thing);
    vm->call_stack = cdr(vm->call_stack);
}
