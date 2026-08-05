#include "vm.h"

#include <assert.h>
#include <stdio.h>

enum instruction {
    INST_PUSH,
    INST_CALL,
    INST_GET,
    INST_POP,
    INST_NOP,
    INST_JMP_IF_NIL,
    INST_GET0,
    INST_SET,
    INST_RET,
    INST_JMP,
    INST_MAKE_ENV2,
    INST_SET_TAG,
    INST_TAG_JMP,
    INST_SET0,
    INST_MAKE_ENV,
    INST_REST_ARGS,
    INST_RAISE,
    INST_SWAP,
    INST_ABORT
};

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

#define CHECK_INSTRUCTION(code, fp, the_arity) \
    if (instruction == LispInt(code)) {        \
        INST##the_arity(fp);                   \
    }
    // clang-format off
    CHECK_INSTRUCTION(INST_PUSH,       vm_inst_push,       1) else
    CHECK_INSTRUCTION(INST_CALL,       vm_inst_call,       0) else
    CHECK_INSTRUCTION(INST_GET,        vm_inst_get,        2) else
    CHECK_INSTRUCTION(INST_POP,        vm_inst_pop,        0) else
    CHECK_INSTRUCTION(INST_NOP,        vm_inst_nop,        0) else
    CHECK_INSTRUCTION(INST_JMP_IF_NIL, vm_inst_jmp_if_nil, 1) else
    CHECK_INSTRUCTION(INST_GET0,       vm_inst_get0,       1) else
    CHECK_INSTRUCTION(INST_SET,        vm_inst_set,        2) else
    CHECK_INSTRUCTION(INST_RET,        vm_inst_ret,        0) else
    CHECK_INSTRUCTION(INST_JMP,        vm_inst_jmp,        1) else
    CHECK_INSTRUCTION(INST_MAKE_ENV2,  vm_inst_setup_env2, 1) else
    CHECK_INSTRUCTION(INST_SET_TAG,    vm_inst_set_tag,    2) else
    CHECK_INSTRUCTION(INST_TAG_JMP,    vm_inst_tag_jmp,    1) else
    CHECK_INSTRUCTION(INST_SET0,       vm_inst_set0,       1) else
    CHECK_INSTRUCTION(INST_MAKE_ENV,   vm_inst_setup_env,  1) else
    CHECK_INSTRUCTION(INST_REST_ARGS,  vm_inst_rest_args,  1) else
    CHECK_INSTRUCTION(INST_RAISE,      vm_inst_raise,      0) else
    CHECK_INSTRUCTION(INST_SWAP,       vm_inst_swap,       0) else
    CHECK_INSTRUCTION(INST_ABORT,      vm_inst_abort,      0) else
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

void vm_inst_push(struct vm *vm, lisp_object_t obj)
{
    assert(vm->top_of_data_stack >= vm->data_stack);
    assert(vm->top_of_data_stack - vm->data_stack < vm->data_stack_size);
    *(vm->top_of_data_stack++) = obj;
}
