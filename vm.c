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
    vm->registers.code_vector = NIL;
    vm->registers.instruction_pointer = 0;
    vm->registers.environment = NIL;
    vm->registers.tags = NIL;
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
    DEFINE_VM_INSTRUCTION(pop,        vm_inst_pop,        0);
    DEFINE_VM_INSTRUCTION(call,       vm_inst_call,       0);
    DEFINE_VM_INSTRUCTION(ret,        vm_inst_ret,        0);
    DEFINE_VM_INSTRUCTION(get,        vm_inst_get,        2);
    DEFINE_VM_INSTRUCTION(set,        vm_inst_set,        2);
    DEFINE_VM_INSTRUCTION(abort,      vm_inst_abort,      0);
    DEFINE_VM_INSTRUCTION(jmp,        vm_inst_jmp,        1);
    DEFINE_VM_INSTRUCTION(jmp_if_nil, vm_inst_jmp_if_nil, 1);
    DEFINE_VM_INSTRUCTION(set_tag,    vm_inst_set_tag,    1);
    DEFINE_VM_INSTRUCTION(tag_jmp,    vm_inst_tag_jmp,    2);
    DEFINE_VM_INSTRUCTION(nop,        vm_inst_nop,        0);

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

lisp_object_t vm_get_stack(struct vm *vm)
{
    lisp_object_t result = NIL;
    for (lisp_object_t *p = vm->top_of_data_stack - 1; p >= vm->data_stack; p--)
        result = cons(*p, result);
    return result;
}

lisp_object_t vm_pop(struct vm *vm)
{
    assert(vm->top_of_data_stack >= vm->data_stack);
    return *(--vm->top_of_data_stack);
}

lisp_object_t vm_peek(struct vm *vm)
{
    assert(vm->top_of_data_stack > vm->data_stack);
    return vm->top_of_data_stack[-1];
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
    } else if (arity == 2 << 4) {
        lisp_object_t arg1 = svref(vm->registers.code_vector, vm->registers.instruction_pointer + 16);
        lisp_object_t arg2 = svref(vm->registers.code_vector, vm->registers.instruction_pointer + 32);
        vm->registers.instruction_pointer += 48;
        ((void (*)(struct vm *, lisp_object_t, lisp_object_t))fp)(vm, arg1, arg2);
        vm_print_stack(vm);
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

void vm_inst_pop(struct vm *vm)
{
    vm->top_of_data_stack--;
}

static void vm_setup_funcall(struct vm *vm)
{
    // Stack for (funcall foo a b c) looks like:
    //   4
    //   c
    //   b
    //   a
    //   foo
    // and we want it to look like this (note same depth):
    //   foo
    //   3
    //   c
    //   b
    //   a
    // old and new values together
    //   4     foo   -1
    //   c     3     -2
    //   b     c     -3
    //   a     b     -4
    //   foo   a     -5   <-- base
    lisp_object_t argcount = vm_peek(vm);
    int argcount_c = argcount >> 4;
    int new_argcount_c = argcount_c - 1;
    lisp_object_t *base = vm->top_of_data_stack - (new_argcount_c + 2);
    vm->top_of_data_stack[-1] = *base;
    for (lisp_object_t *p = base; p < vm->top_of_data_stack - 1; p++) {
        p[0] = p[1];
    }
    vm->top_of_data_stack[-2] = new_argcount_c << 4;
}

static void vm_call_builtin_function(struct vm *vm, struct lisp_function *fnptr)
{
    lisp_object_t result = NIL;
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
        result = ((lisp_object_t (*)())fp)();
        break;
    case 1:
        arg1 = vm_pop(vm);
        result = ((lisp_object_t (*)(lisp_object_t))fp)(arg1);
        break;
    case 2:
        arg2 = vm_pop(vm);
        arg1 = vm_pop(vm);
        result = ((lisp_object_t (*)(lisp_object_t, lisp_object_t))fp)(arg1, arg2);
        break;
    case 3:
        arg3 = vm_pop(vm);
        arg2 = vm_pop(vm);
        arg1 = vm_pop(vm);
        result = ((lisp_object_t (*)(lisp_object_t, lisp_object_t, lisp_object_t))fp)(arg1, arg2, arg3);
        break;
    default:
        abort();
    }
    vm_inst_push(vm, result);
}

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

static void vm_call_lambda(struct vm *vm, struct lisp_function *fnptr)
{
    lisp_object_t n_args = vm_pop(vm);
    assert(integerp(n_args) != NIL);
    int n_args_int = n_args >> 4;

    lisp_object_t actual_function = fnptr->actual_function;
    lisp_object_t arg_info = cadr(actual_function);
    TRACE(arg_info);
    assert(vectorp(arg_info) != NIL);
    lisp_object_t rest_args = svref_c(arg_info, 0);
    lisp_object_t arity = svref_c(arg_info, 1);
    int arity_int = arity >> 4;
    TRACE(rest_args);
    TRACE(arity);
    lisp_object_t env_size = arity + 0x10;
    if (rest_args != NIL)
        /* Add a slot for the rest args */
        env_size += 0x10;
    lisp_object_t env = allocate_vector(env_size);
    lisp_object_t actual_rest_args = NIL;
    if (rest_args != NIL) {
        /* arity does not include the rest args */
        int rest_arg_count = n_args_int - arity_int;
        for (int i = 0; i < rest_arg_count; i++) {
            actual_rest_args = cons(vm_pop(vm), actual_rest_args);
            TRACE(actual_rest_args);
        }
    }
    for (int i = arity >> 4; i > 0; i--) {
        svref_set(env, i << 4, vm_pop(vm));
    }
    if (rest_args != NIL)
        svref_set(env, arity + 0x10, actual_rest_args);
    svref_set(env, 0, car(fnptr->actual_function));
    *vm->call_stack_pointer = vm->registers;
    vm->call_stack_pointer++;

    vm->registers.code_vector = caddr(actual_function);
    vm->registers.instruction_pointer = 0;
    vm->registers.environment = env;
    vm->registers.tags = NIL;
}

void vm_inst_call(struct vm *vm)
{
    lisp_object_t fn = NIL;
start:
    fn = vm_pop(vm);
    if (fn == sym("funcall")) {
        vm_setup_funcall(vm);
        goto start;
    }
    /* For now at least, you can call a symbol.  This makes it easier
       to write VM code by hand. */
    if (symbolp(fn) != NIL)
        fn = (SymbolPtr(fn))->function;
    struct lisp_function *fnptr = LispFunctionPtr(fn);
    if (fnptr->kind == interp->syms.built_in_function) {
        vm_call_builtin_function(vm, fnptr);
    } else if (fnptr->kind == interp->syms.lambda) {
        vm_call_lambda(vm, fnptr);
    } else {
        abort();
    }
}

void vm_inst_ret(struct vm *vm)
{
    assert(vm->call_stack_pointer > vm->call_stack);
    struct vm_call_stack_frame *call_stack_frame = --vm->call_stack_pointer;
    vm->registers = *call_stack_frame;
}

static lisp_object_t findenv(lisp_object_t env, int offset)
{
    check_vector(env); // Eventually this should raise an exception VM-style, not interpreter-style
    for (int i = 0; i < offset; i++) {
        lisp_object_t parent = svref(env, 0);
        if (parent == NIL)
            abort();
        env = parent;
    }
    return env;
}

void vm_inst_get(struct vm *vm, lisp_object_t n, lisp_object_t m)
{
    lisp_object_t env = findenv(vm->registers.environment, n >> 4);
    assert(length(env) > m);
    vm_inst_push(vm, svref(env, m));
}

void vm_inst_set(struct vm *vm, lisp_object_t n, lisp_object_t m)
{
    lisp_object_t env = findenv(vm->registers.environment, n >> 4);
    assert(length(env) > m);
    /* peek not pop here since we return the new value */
    svref_set(env, m, vm_peek(vm));
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

lisp_object_t vm_make_function(lisp_object_t arg_info, lisp_object_t code)
{
    lisp_object_t fn = allocate_function();
    struct lisp_function *fnptr = LispFunctionPtr(fn);
    fnptr->actual_function = List(interp->vm.registers.environment, arg_info, code);
    fnptr->kind = interp->syms.lambda;
    return fn;
}

static lisp_object_t frame_has_tag(struct vm_call_stack_frame *frame, lisp_object_t tag)
{
    for (lisp_object_t t = frame->tags; t != NIL; t = cdr(t)) {
        if (eq(caar(t), tag) != NIL) {
            return cdar(t);
        }
    }
    return NIL;
}

void vm_inst_set_tag(struct vm *vm, lisp_object_t tag, lisp_object_t dest)
{
    vm->registers.tags = cons(cons(tag, dest), vm->registers.tags);
}

void vm_inst_tag_jmp(struct vm *vm, lisp_object_t tag)
{
    lisp_object_t dest = frame_has_tag(&vm->registers, tag);
    if (dest != NIL) {
        vm_inst_jmp(vm, dest);
        return;
    }
    for (struct vm_call_stack_frame *frame = vm->call_stack_pointer - 1; frame >= vm->call_stack; frame--) {
        lisp_object_t dest = frame_has_tag(frame, tag);
        if (dest != NIL) {
            vm->call_stack_pointer = frame + 1;
            vm->registers = *frame;
            vm_inst_jmp(vm, dest);
            return;
        }
    }
    abort();
}

void vm_inst_nop(struct vm *vm)
{
}
