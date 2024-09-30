#include "vm.h"
#include "interp.h"

#include <assert.h>
#include <stdio.h>

static void initialize_data_stack(struct vm *vm)
{
    for (size_t x = 0; x < vm->size; x++) {
        lisp_object_t *p = vm->top_of_data_stack + x;
        *p = (lisp_object_t)12345678;
    }
}

void vm_init(struct vm *vm, size_t data_stack_size)
{
    vm->size = data_stack_size;
    vm->data_stack = (lisp_object_t *)malloc(sizeof(lisp_object_t) * vm->size);
    vm->call_stack = (struct vm_call_stack_frame *)malloc(sizeof(struct vm_call_stack_frame) * 1024);
    vm->call_stack_pointer = vm->call_stack;
    vm->top_of_data_stack = vm->data_stack;
    vm->registers.frame_pointer = vm->data_stack;
    vm->registers.code_vector = NIL;
    vm->registers.instruction_pointer = 0;
    initialize_data_stack(vm);
}

void vm_free(struct vm *vm)
{
    free(vm->data_stack);
    free(vm->call_stack);
}

static void define_vm_instruction(lisp_object_t symbol, void (*function_pointer)(void), int arity)
{
    putprop(symbol, interp->syms.vm_ins_fp, (((lisp_object_t)function_pointer) << 4) | FUNCTION_POINTER_TYPE);
    putprop(symbol, interp->syms.vm_ins_arity, ((uint64_t)arity) << 4);
}

void init_vm_instruction_definitions()
{
#define DEFINE_VM_INSTRUCTION(S, F, A) define_vm_instruction(interp->syms.S, (void (*)(void))F, A)
    // clang-format off
    DEFINE_VM_INSTRUCTION(push,       vm_inst_push,       1);
    //DEFINE_VM_INSTRUCTION(copy,       vm_inst_copy,       1);
    DEFINE_VM_INSTRUCTION(swap_pop,   vm_inst_swap_pop,   1);
    DEFINE_VM_INSTRUCTION(swap,       vm_inst_swap,       0);
    DEFINE_VM_INSTRUCTION(pop,        vm_inst_pop,        0);
    DEFINE_VM_INSTRUCTION(call,       vm_inst_call,       1);
    DEFINE_VM_INSTRUCTION(ret,        vm_inst_ret,        0);
    DEFINE_VM_INSTRUCTION(copy2,      vm_inst_copy2,      1);
    DEFINE_VM_INSTRUCTION(abort,      vm_inst_abort,      0);
    DEFINE_VM_INSTRUCTION(jmp,        vm_inst_jmp,        1);
    DEFINE_VM_INSTRUCTION(jmp_if_nil, vm_inst_jmp_if_nil, 1);
    // clang-format on
#undef DEFINE_VM_INSTRUCTION
}

void vm_print_stack(struct vm *vm)
{
    /* Visually speaking, stack grows up here: */
    printf("STACK ->\n");
    int i = 0;
    for (lisp_object_t *p = vm->top_of_data_stack - 1; p >= vm->data_stack; p--) {
        lisp_object_t obj = *p;
        char *str = print_object(obj);
        printf("%d %s\n", i, str);
        free(str);
        i++;
    }
    printf("<- STACK\n");
}

lisp_object_t vm_pop(struct vm *vm)
{
    assert(vm->top_of_data_stack >= vm->data_stack);
    return *(--vm->top_of_data_stack);
}

lisp_object_t vm_peek(struct vm *vm)
{
    assert(vm->top_of_data_stack > vm->data_stack);
    return *(vm->top_of_data_stack - 1);
}

void vm_run_one_instruction(struct vm *vm)
{
    vm_print_stack(vm);
    TRACE(vm->registers.instruction_pointer);
    TRACE(vm->registers.code_vector);
    lisp_object_t instruction = svref(vm->registers.code_vector, vm->registers.instruction_pointer);
    lisp_object_t arity = getprop(instruction, interp->syms.vm_ins_arity);
    TRACE(instruction);
    if (arity == NIL)
        abort();
    lisp_object_t lisp_function_pointer = getprop(instruction, interp->syms.vm_ins_fp);
    if (lisp_function_pointer == NIL)
        abort();
    void (*fp)() = FunctionPtr(lisp_function_pointer);
    if (arity == 0) {
        vm->registers.instruction_pointer += 16;
        ((void (*)(struct vm *))fp)(vm);
    } else if (arity == 1 << 4) {
        lisp_object_t arg = svref(vm->registers.code_vector, vm->registers.instruction_pointer + 16);
        vm->registers.instruction_pointer += 32;
        ((void (*)(struct vm *, lisp_object_t))fp)(vm, arg);
    } else {
        abort();
    }
}

void vm_run(struct vm *vm)
{
    while (vm->registers.instruction_pointer < length(vm->registers.code_vector))
        vm_run_one_instruction(vm);
}

lisp_object_t vm_eval(lisp_object_t code_vector)
{
    interp->vm.registers.code_vector = code_vector;
    interp->vm.registers.instruction_pointer = 0;
    vm_run(&interp->vm);
    return vm_pop(&interp->vm);
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

void vm_inst_copy2(struct vm *vm, lisp_object_t offset)
{
    vm_inst_push(vm, *(vm->registers.frame_pointer - (offset >> 4)));
}

void vm_inst_swap_pop(struct vm *vm, lisp_object_t n)
{
    lisp_object_t new_top = *(vm->top_of_data_stack - 1);
    vm->top_of_data_stack -= n >> 4;
    *(vm->top_of_data_stack - 1) = new_top;
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

void vm_inst_call(struct vm *vm, lisp_object_t n)
{
    lisp_object_t fn = vm_pop(vm);
    /* For now at least, you can call a symbol.  This makes it easier to write VM code by hand. */
    if (symbolp(fn) != NIL)
        fn = (SymbolPtr(fn))->function;
    struct lisp_function *fnptr = LispFunctionPtr(fn);
    lisp_object_t result = NIL;
    if (fnptr->kind == interp->syms.built_in_function) {
        lisp_object_t actual_function = fnptr->actual_function;
        void (*fp)() = FunctionPtr(cadr(actual_function));
        lisp_object_t arg1 = NIL;
        lisp_object_t arg2 = NIL;
        lisp_object_t arg3 = NIL;
        lisp_object_t provided_arity = vm_pop(vm);
        lisp_object_t arity = (int64_t)caddr(actual_function);
        if (provided_arity != arity)
            abort();
        int arity_c = arity >> 4;
        switch (arity_c) {
        case 0:
            result = ((lisp_object_t(*)())fp)();
            break;
        case 1:
            arg1 = vm_pop(vm);
            result = ((lisp_object_t(*)(lisp_object_t))fp)(arg1);
            break;
        case 2:
            arg2 = vm_pop(vm);
            arg1 = vm_pop(vm);
            result = ((lisp_object_t(*)(lisp_object_t, lisp_object_t))fp)(arg1, arg2);
            break;
        case 3:
            arg3 = vm_pop(vm);
            arg2 = vm_pop(vm);
            arg1 = vm_pop(vm);
            result = ((lisp_object_t(*)(lisp_object_t, lisp_object_t, lisp_object_t))fp)(arg1, arg2, arg3);
            break;
        default:
            abort();
        }
        vm_inst_push(vm, result);
    } else if (fnptr->kind == interp->syms.lambda) {
        *vm->call_stack_pointer = vm->registers;
        vm->call_stack_pointer++;
        vm->registers.code_vector = fnptr->actual_function;
        vm->registers.instruction_pointer = 0;
        // maybe make this a separate instruction and do it before pushing the args
        // what else would we have to do
        vm->registers.frame_pointer = vm->top_of_data_stack;
        printf("INTERESTING2 %p\n", vm->registers.frame_pointer);
    } else {
        abort();
    }
}

void vm_inst_ret(struct vm *vm)
{
    assert(vm->call_stack_pointer > vm->call_stack);
    struct vm_call_stack_frame *call_stack_frame = --vm->call_stack_pointer;
    printf("INTERESTING %p %p\n", call_stack_frame->frame_pointer, vm->top_of_data_stack);
    vm->registers = *call_stack_frame;
}

lisp_object_t vm_make_function(lisp_object_t code_vector) // move this
{
    lisp_object_t fn = allocate_function();
    struct lisp_function *fnptr = LispFunctionPtr(fn);
    fnptr->kind = interp->syms.lambda;
    fnptr->actual_function = code_vector;
    return fn;
}

void vm_inst_abort(struct vm *vm)
{
    abort();
}

void vm_inst_jmp(struct vm *vm, lisp_object_t dest)
{
    vm->registers.instruction_pointer = dest;
}

void vm_inst_jmp_if_nil(struct vm *vm, lisp_object_t dest)
{
    lisp_object_t value = vm_pop(vm);
    if (value == NIL)
        vm->registers.instruction_pointer = dest;
}

/*
What about setting things?
New value on top of data stack
Special instruction to set a stack location to value on top of stack?
 */
