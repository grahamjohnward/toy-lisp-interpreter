#include "vm.h"
#include "interp.h"

#include <assert.h>
#include <setjmp.h>
#include <stdio.h>

void vm_init(struct vm *vm, size_t data_stack_size)
{
    vm->data_stack_size = data_stack_size;
    vm->data_stack = (lisp_object_t *)malloc(sizeof(lisp_object_t) * vm->data_stack_size);
    vm->other_data_stack = (lisp_object_t *)malloc(sizeof(lisp_object_t) * vm->data_stack_size);

    vm->call_stack_size = 1024 * 1024;
    vm->call_stack = (struct vm_call_stack_frame *)malloc(sizeof(struct vm_call_stack_frame) * vm->call_stack_size);
    vm->call_stack_pointer = vm->call_stack;
    vm->top_of_data_stack = vm->data_stack;
    vm->registers.code_vector = NIL;
    vm->registers.environment = NIL;
    vm->registers.closure_env = NIL;
    vm->registers.tags = NIL;
    vm->registers.fp = NULL;
    vm->registers.sp = vm->other_data_stack;
    vm->vm_trace = 0;
#ifdef VM_TRACE_ENABLED
    vm->vm_trace_in_this_build = 1;
#else
    vm->vm_trace_in_this_build = 0;
#endif
    vm->setjmp_activated = 0;
}

void vm_free(struct vm *vm)
{
    free(vm->data_stack);
    free(vm->call_stack);
}

static void gc_copy_vm_data_stack(struct lisp_heap *heap, struct vm *vm)
{
    for (lisp_object_t *p = vm->data_stack; p < vm->top_of_data_stack; p++)
        gc_copy(heap, p);
}

static void gc_copy_vm_other_stack(struct lisp_heap *heap, struct vm *vm)
{
    for (lisp_object_t *p = vm->other_data_stack; p < vm->registers.sp; p++)
        gc_copy(heap, p);
}

static void gc_copy_vm_call_stack_frame(struct lisp_heap *heap, struct vm_call_stack_frame *frame)
{
    gc_copy(heap, &frame->code_vector);
    ptrdiff_t instruction_pointer_offset = frame->instruction_pointer - frame->code_vector_storage;
    if (frame->code_vector != NIL) {
        frame->code_vector_storage = get_vector_storage(frame->code_vector);
        size_t code_vector_size = length_c(frame->code_vector);
        lisp_object_t *vector_storage = get_vector_storage(frame->code_vector);
        frame->max_instruction_pointer = vector_storage + code_vector_size;
    }
    frame->instruction_pointer = frame->code_vector_storage + instruction_pointer_offset;
    gc_copy(heap, &frame->environment);
    gc_copy(heap, &frame->closure_env);
    gc_copy(heap, &frame->tags);
}

static void gc_copy_vm_call_stack(struct lisp_heap *heap, struct vm *vm)
{
    for (struct vm_call_stack_frame *p = vm->call_stack; p < vm->call_stack_pointer; p++)
        gc_copy_vm_call_stack_frame(heap, p);
}

void gc_copy_vm(struct lisp_heap *heap, struct vm *vm)
{
    gc_copy_vm_data_stack(heap, vm);
    gc_copy_vm_other_stack(heap, vm);
    gc_copy_vm_call_stack(heap, vm);
    gc_copy_vm_call_stack_frame(heap, &vm->registers);
    if (vm->setjmp_activated)
        gc_copy_jmp_buf(heap, vm->jmp_buf);
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

static void vm_print_other_stack(struct vm *vm)
{
    /* Visually speaking, stack grows up here: */
    printf("OTHER STACK ->\n");
    int i = 0;
    for (lisp_object_t *p = vm->registers.sp - 1; p >= vm->other_data_stack; p--) {
        lisp_object_t obj = *p;
        char *str = print_object(obj);
        printf(". %d %s\n", i, str);
        free(str);
        i++;
    }
    printf("<- OTHER STACK\n");
}

static void vm_print_call_stack_frame(struct vm_call_stack_frame *p)
{
    lisp_object_t vector = allocate_vector(LispInt(5));
    svref_set(vector, LispInt(0), p->code_vector);
    svref_set(vector, LispInt(2), p->environment);
    svref_set(vector, LispInt(3), LispInt(p->instruction_pointer - p->code_vector_storage));
    svref_set(vector, LispInt(4), p->tags);
    char *str = print_object(vector);
    printf("%s\n", str);
    free(str);
}

void vm_print_call_stack(struct vm *vm, char *message)
{
    printf("CALL STACK %s ->\n", message);
    vm_print_call_stack_frame(&vm->registers);
    for (struct vm_call_stack_frame *p = vm->call_stack_pointer - 1; p >= vm->call_stack; p--) {
        vm_print_call_stack_frame(p);
    }
    printf("<- CALL STACK\n");
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

#ifdef VM_TRACE_ENABLED
#define TRACE0 VM_TRACE("; %p %s\t%s\n", (void *)vm->registers.code_vector, str0, str1)
#define TRACE1                     \
    if (vm->vm_trace)              \
        str2 = print_object(arg1); \
    VM_TRACE("; %p %s\t%s %s\n", (void *)vm->registers.code_vector, str0, str1, str2)
#define TRACE2                     \
    if (vm->vm_trace) {            \
        str2 = print_object(arg1); \
        str3 = print_object(arg2); \
    }                              \
    VM_TRACE("; %p %s\t%s %s %s\n", (void *)vm->registers.code_vector, str0, str1, str2, str3)
#else
#define TRACE0 \
    do {       \
    } while (0);
#define TRACE1 \
    do {       \
    } while (0);
#define TRACE2 \
    do {       \
    } while (0);
#endif

#define VM_TRACE(format, ...)        \
    if (interp->vm.vm_trace) {       \
        printf(format, __VA_ARGS__); \
    }

#define INST0(fp)                        \
    vm->registers.instruction_pointer++; \
    TRACE0;                              \
    fp(vm);

#define INST1(fp)                                \
    arg1 = vm->registers.instruction_pointer[1]; \
    vm->registers.instruction_pointer += 2;      \
    TRACE1;                                      \
    fp(vm, arg1);

#define INST2(fp)                                \
    arg1 = vm->registers.instruction_pointer[1]; \
    arg2 = vm->registers.instruction_pointer[2]; \
    vm->registers.instruction_pointer += 3;      \
    TRACE2;                                      \
    fp(vm, arg1, arg2);

void vm_run_one_instruction(struct vm *vm)
{
#ifdef VM_TRACE_ENABLED
    if (vm->vm_trace && (vm->registers.instruction_pointer - vm->registers.code_vector_storage) == 0)
        TRACE(vm->registers.code_vector);
#endif

    lisp_object_t instruction = *vm->registers.instruction_pointer;
    lisp_object_t arg1 = NIL;
    lisp_object_t arg2 = NIL;

#ifdef VM_TRACE_ENABLED
    char *str0 = NULL;
    char *str1 = NULL;
    char *str2 = NULL;
    char *str3 = NULL;
    if (vm->vm_trace) {
        str0 = print_object(LispInt(vm->registers.instruction_pointer - vm->registers.code_vector_storage));
        str1 = print_object(instruction);
    }
#endif

#define CHECK_INSTRUCTION(sym, fp, the_arity) \
    if (instruction == interp->syms.sym) {    \
        INST##the_arity(fp);                  \
    }
    // clang-format off
    CHECK_INSTRUCTION(push,       vm_inst_push,       1) else
    CHECK_INSTRUCTION(call,       vm_inst_call,       0) else
    CHECK_INSTRUCTION(get,        vm_inst_get,        2) else
    CHECK_INSTRUCTION(pop,        vm_inst_pop,        0) else
    CHECK_INSTRUCTION(nop,        vm_inst_nop,        0) else
    CHECK_INSTRUCTION(jmp_if_nil, vm_inst_jmp_if_nil, 1) else
    CHECK_INSTRUCTION(get0,       vm_inst_get0,       1) else
    CHECK_INSTRUCTION(set,        vm_inst_set,        2) else
    CHECK_INSTRUCTION(ret,        vm_inst_ret,        0) else
    CHECK_INSTRUCTION(jmp,        vm_inst_jmp,        1) else
    CHECK_INSTRUCTION(make_env2,  vm_inst_setup_env2, 1) else
    CHECK_INSTRUCTION(set_tag,    vm_inst_set_tag,    2) else
    CHECK_INSTRUCTION(tag_jmp,    vm_inst_tag_jmp,    1) else
    CHECK_INSTRUCTION(set0,       vm_inst_set0,       1) else
    CHECK_INSTRUCTION(make_env,   vm_inst_setup_env,  1) else
    CHECK_INSTRUCTION(rest_args,  vm_inst_rest_args,  1) else
    CHECK_INSTRUCTION(raise,      vm_inst_raise,      0) else
    CHECK_INSTRUCTION(swap,       vm_inst_swap,       0) else
    CHECK_INSTRUCTION(abort,      vm_inst_abort,      0) else
    // clang-format on
    {
        TRACE(instruction);
        abort();
    }

#undef CHECK_INSTRUCTION
#ifdef VM_TRACE_ENABLED
    if (vm->vm_trace) {
        free(str0);
        free(str1);
        if (str2)
            free(str2);
        if (str3)
            free(str3);
    }
#endif
}

#undef INST2

#undef INST1

#undef INST0

#undef VM_TRACE

void vm_run(struct vm *vm)
{
    while (vm->registers.instruction_pointer < vm->registers.max_instruction_pointer)
        vm_run_one_instruction(vm);
    vm_print_stack(vm);
}

/** Instructions **/

void vm_inst_push(struct vm *vm, lisp_object_t obj)
{
    assert(vm->top_of_data_stack >= vm->data_stack);
    assert(vm->top_of_data_stack - vm->data_stack < vm->data_stack_size);
    *(vm->top_of_data_stack++) = obj;
}

void vm_inst_pop(struct vm *vm)
{
    vm->top_of_data_stack--;
    assert(vm->top_of_data_stack >= vm->data_stack);
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
    int argcount_c = Int(argcount);
    int new_argcount_c = argcount_c - 1;
    lisp_object_t *base = vm->top_of_data_stack - (new_argcount_c + 2);
    vm->top_of_data_stack[-1] = *base;
    for (lisp_object_t *p = base; p < vm->top_of_data_stack - 1; p++) {
        p[0] = p[1];
    }
    vm->top_of_data_stack[-2] = LispInt(new_argcount_c);
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
    if (provided_arity != arity) {
        vm_inst_push(vm, sym("bad-arity"));
        vm_inst_push(vm, cons(provided_arity, arity));
        vm_inst_push(vm, LispInt(2));
        vm_inst_raise(vm);
        return;
    }
    int v = setjmp(vm->jmp_buf);
    if (v != 0) {
        vm_inst_raise(vm);
        vm->setjmp_activated = 0;
        return;
    } else {
        vm->setjmp_activated = 1;
    }
    int arity_c = Int(arity);
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
        printf("Bad arity: %d\n", arity_c);
        abort();
    }
    vm->setjmp_activated = 0;
    vm_inst_push(vm, result);
}

static void vm_handle_rest_args(struct vm *vm, lisp_object_t arity)
{
    lisp_object_t actual_arg_count = vm_pop(vm);
    assert(integerp(actual_arg_count) != NIL);
    int actual_arg_count_int = Int(actual_arg_count);

    lisp_object_t actual_rest_args = NIL;
    int args_left = actual_arg_count_int;

    int rest_arg_count = 1 + actual_arg_count_int - Int(arity);
    for (int i = 0; i < rest_arg_count; i++) {
        actual_rest_args = cons(vm_pop(vm), actual_rest_args);
        args_left--;
    }
    vm_inst_push(vm, actual_rest_args);
    vm_inst_push(vm, LispInt(args_left + 1));
}

void vm_inst_rest_args(struct vm *vm, lisp_object_t arity)
{
    vm_handle_rest_args(vm, arity);
}

void vm_inst_setup_env(struct vm *vm, lisp_object_t arity)
{
    lisp_object_t arg_count = vm_pop(vm);
    vm->registers.environment = allocate_vector(arity + LispInt(1));
    for (int i = Int(arg_count); i > 0; i--)
        svref_set(vm->registers.environment, LispInt(i), vm_pop(vm));
    svref_set(vm->registers.environment, 0, vm->registers.closure_env);
}

void vm_inst_setup_env2(struct vm *vm, lisp_object_t arity)
{
    lisp_object_t arg_count = vm_pop(vm);
    if (arg_count > arity)
        abort();
    /* We have arg_count arguments on the stack and need to populate
       arity slots on the other stack */
    vm->registers.fp = vm->registers.sp;
    for (int i = Int(arg_count) - 1; i >= 0; i--)
        vm->registers.fp[i] = vm_pop(vm);
    for (int i = Int(arity) - 1; i > Int(arg_count) - 1; i--)
        vm->registers.fp[i] = NIL;
    vm->registers.sp = vm->registers.fp + Int(arity);
    if (vm->registers.closure_env != NIL) {
        vm->registers.environment = allocate_vector(LispInt(1));
        svref_set(vm->registers.environment, 0, vm->registers.closure_env);
    } else {
        vm->registers.environment = NIL;
    }
}

void vm_set_code_vector(struct vm *vm, lisp_object_t code_vector)
{
    vm->registers.code_vector = code_vector;
    size_t code_vector_size = length_c(vm->registers.code_vector);
    lisp_object_t *vector_storage = get_vector_storage(vm->registers.code_vector);
    vm->registers.instruction_pointer = vector_storage;
    vm->registers.max_instruction_pointer = vector_storage + code_vector_size;
    vm->registers.code_vector_storage = vector_storage;
}

static void vm_call_lambda(struct vm *vm, struct lisp_function *fnptr)
{
    *vm->call_stack_pointer = vm->registers;
    vm->call_stack_pointer++;
    vm_set_code_vector(vm, caddr(fnptr->actual_function));
    vm->registers.environment = NIL;
    vm->registers.closure_env = car(fnptr->actual_function);
    vm->registers.tags = NIL;
}

void vm_inst_call(struct vm *vm)
{
    lisp_object_t fn = NIL;
    int funcall_count = 0;
start:
    fn = vm_pop(vm);
    lisp_object_t orig_fn = NIL;
    orig_fn = fn;
    if (fn == interp->syms.funcall) {
        vm_setup_funcall(vm);
        funcall_count++;
        goto start;
    }
    /* For now at least, you can call a symbol.  This makes it easier
       to write VM code by hand. */
    if (symbolp(fn) != NIL) {
        fn = (SymbolPtr(fn))->function;
    }
    if (functionp(fn) == NIL) {
        vm_inst_push(vm, sym("bad-function"));
        vm_inst_push(vm, orig_fn);
        vm_inst_push(vm, LispInt(2));
        vm_inst_raise(vm);
        return;
    }
    struct lisp_function *fnptr = LispFunctionPtr(fn);

    if (fnptr->kind == interp->syms.built_in_function) {
        vm_call_builtin_function(vm, fnptr);
    } else if (fnptr->kind == interp->syms.lambda) {
        vm_call_lambda(vm, fnptr);
    } else {
        char *str = print_object(fn);
        printf("Bad function: %s\n", str);
        free(str);
        str = print_object(fnptr->kind);
        printf("Bad kind: %s\n", str);
        vm_print_stack(vm);
        free(str);
        vm_inst_push(vm, sym("bad-function"));
        vm_inst_push(vm, fn);
        vm_inst_push(vm, LispInt(2));
        vm_inst_raise(vm);
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
    lisp_object_t env = NIL;
    if (n > 0)
        /* We can just get it from closure_env register */
        env = findenv(vm->registers.closure_env, Int(n) - 1);
    else
        /* Old logic: transitional case where "get 0 n" used rather than get0 n */
        env = findenv(vm->registers.environment, Int(n));
    assert(length(env) > m);
    vm_inst_push(vm, svref(env, m));
}

void vm_inst_set(struct vm *vm, lisp_object_t n, lisp_object_t m)
{
    lisp_object_t env = findenv(vm->registers.environment, Int(n));
    assert(length(env) > m);
    /* peek not pop here since we return the new value */
    svref_set(env, m, vm_peek(vm));
}

void vm_inst_abort(struct vm *vm)
{
    vm_print_stack(vm);
    vm_print_call_stack(vm, "aborted");
    abort();
}

void vm_inst_jmp(struct vm *vm, lisp_object_t dest)
{
    vm->registers.instruction_pointer = vm->registers.code_vector_storage + Int(dest);
}

void vm_inst_jmp_if_nil(struct vm *vm, lisp_object_t dest)
{
    lisp_object_t value = vm_pop(vm);
    if (value == NIL) {
        vm->registers.instruction_pointer = vm->registers.code_vector_storage + Int(dest);
    }
}

lisp_object_t vm_make_function2(lisp_object_t arg_info, lisp_object_t code, lisp_object_t env)
{
    lisp_object_t fn = allocate_function();
    lisp_object_t actual_function = List(env, arg_info, code);
    struct lisp_function *fnptr = LispFunctionPtr(fn);
    fnptr->actual_function = actual_function;
    fnptr->kind = interp->syms.lambda;
    fnptr->name = code;
    return fn;
}

lisp_object_t vm_make_function(lisp_object_t arg_info, lisp_object_t code)
{
    return vm_make_function2(arg_info, code, interp->vm.registers.environment);
}

lisp_object_t vm_make_simple_function(lisp_object_t arg_info, lisp_object_t code)
{
    return vm_make_function2(arg_info, code, NIL);
}

static lisp_object_t frame_has_tag(struct vm_call_stack_frame *frame, lisp_object_t tag)
{
    for (lisp_object_t t = frame->tags; t != NIL; t = cdr(t)) {
        if (eq(svref_c(car(t), 0), tag) != NIL) {
            return car(t);
        }
    }
    return NIL;
}

void vm_inst_set_tag(struct vm *vm, lisp_object_t tag, lisp_object_t dest)
{
    ptrdiff_t stack_offset_c = vm->top_of_data_stack - vm->data_stack;
    lisp_object_t stack_offset = LispInt(stack_offset_c);
    lisp_object_t tag_info = allocate_vector(LispInt(3));
    svref_set(tag_info, 0, tag);
    svref_set(tag_info, LispInt(1), dest);
    svref_set(tag_info, LispInt(2), stack_offset);
    vm->registers.tags = cons(tag_info, vm->registers.tags);
}
// Do we also need an instruction to clear a tag?  Think so ...

void vm_inst_tag_jmp(struct vm *vm, lisp_object_t tag)
{
    lisp_object_t tag_info = frame_has_tag(&vm->registers, tag);
    // These seem like two different behaviours according to whether the tag
    // lives in the current call stack frame or not.  The ONLY uses of tag-jmp
    // are:
    // 1.  Implementation of `(go ...)` in `tagbody`
    // 2.  Native exceptions i.e. `raise()` called in native code.
    if (tag_info != NIL) {
        vm_inst_jmp(vm, svref_c(tag_info, 1));
        return;
    }
    // Is restoring data stack etc. actually the right behaviour when this is
    // happening from (go ...) inside a tagbody?  Maybe it is actually.  Needs a
    // test case.  For now, let's assume this is OK for the tagbody use-case
    for (struct vm_call_stack_frame *frame = vm->call_stack_pointer - 1; frame >= vm->call_stack; frame--) {
        lisp_object_t tag_info = frame_has_tag(frame, tag);
        if (tag_info != NIL) {
            lisp_object_t dest = svref_c(tag_info, 1);
            lisp_object_t stack_offset = svref_c(tag_info, 2);
            vm->call_stack_pointer = frame;
            vm->registers = *frame;
            vm->top_of_data_stack = vm->data_stack + Int(stack_offset);
            vm_inst_jmp(vm, dest);
            return;
        }
    }
    char *str = print_object(tag);
    printf("No handler for %s\n", str);
    free(str);
    exit(1);
}

void vm_inst_raise(struct vm *vm)
{
    // So the idea now is that this instruction will be the (entire) body of the
    // RAISE function, and tag and value should come from the stack.  I think it
    // needs to be called as a built-in function not as a lambda.  Aha, but the
    // built-in function calling machinery already unpacks the stack as below,
    // so it's not simply a built-in.  How to do this?
    //
    // One way would be to change the calling convention for lambdas to put the
    // environment set up in instructions rather than C code.
    //
    // Another would be just to eat the lambda overhead.
    //
    // Another option is just to make this a built-in that calls vm_inst_tag_jmp
    // followed by vm_inst_push
    //
    // Yet another option is to separate RAISE the function (which needs to be
    // funcallable) from RETURN-FROM which can be a compiler intrinsic.
    //
    // As a compiler intrinsic, this is
    //   push <tag>  ; (known statically)
    //   <evaluate value> ; (left on stack)
    //   push 2
    //   raise
    // Now what about the funcallable version?  Can we build it with this
    // instruction?  Yup, something like (raise 'tag value) =>
    //   get 0
    //   get 1
    //   push 2
    //   raise
    lisp_object_t argcount = vm_pop(vm);
    assert(Int(argcount) == 2);
    lisp_object_t value = vm_pop(vm);
    lisp_object_t tag = vm_pop(vm);

    vm_inst_tag_jmp(vm, tag);
    vm_inst_push(vm, value);
}

void vm_inst_nop(struct vm *vm)
{
}

void vm_inst_swap(struct vm *vm)
{
    lisp_object_t top = vm_pop(vm);
    lisp_object_t next = vm_pop(vm);
    vm_inst_push(vm, top);
    vm_inst_push(vm, next);
}

void vm_inst_set_fp(struct vm *vm)
{
    lisp_object_t actual_arg_count = vm_pop(vm);
    assert(integerp(actual_arg_count) != NIL);
    vm->registers.fp = vm->top_of_data_stack - Int(actual_arg_count);
}

void vm_inst_get0(struct vm *vm, lisp_object_t n)
{
    vm_inst_push(vm, vm->registers.fp[Int(n) - 1]);
}

void vm_inst_set0(struct vm *vm, lisp_object_t n)
{
    vm->registers.fp[Int(n) - 1] = vm_peek(vm);
}
