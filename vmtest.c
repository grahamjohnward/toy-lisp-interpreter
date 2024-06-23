#include "lisp.h"

#include "vm.h"

#include <stdio.h>

static void test_push_pop()
{
    struct vm vm;
    vm_init(&vm, 1024);
    init_interpreter(1024 * 1024);
    vm_run_instruction(&vm, List(sym("push"), sym("foo")));
    vm_run_instruction(&vm, List(sym("push"), sym("bar")));

    char *boo = print_object(vm_pop2(&vm));
    printf("%s\n", boo);
    free(boo);

    boo = print_object(vm_pop2(&vm));
    printf("%s\n", boo);
    free(boo);

    printf("%d\n", vm.top_of_data_stack == vm.data_stack);

    vm_free(&vm);
    free_interpreter();
}

static void test_call()
{
    init_interpreter(1024 * 1024 * 4);
    struct vm vm;
    vm_init(&vm, 1024);

    lisp_object_t code_vector = allocate_vector(4 << 4);
    svref_set(code_vector, 0 << 4, List(sym("push"), sym("foo")));
    svref_set(code_vector, 1 << 4, List(sym("push"), sym("bar")));
    struct symbol *bof = SymbolPtr(sym("cons"));
    svref_set(code_vector, 2 << 4, List(sym("push"), bof->function));
    svref_set(code_vector, 3 << 4, sym("call"));
    vm.current_code_vector = code_vector;
    vm_run(&vm);
    char *boo = print_object(vm_peek(&vm));
    printf("%s\n", boo);
    free(boo);

    free_interpreter();
    vm_free(&vm);
}

static void test_function()
{
    init_interpreter(1024 * 1024 * 4);
    struct vm vm;
    vm_init(&vm, 1024);

    // (lambda (x) (cons 'hello x))
    // Stack when we enter lambda:
    //   x
    // Stack when we call cons:
    //   #'cons
    //   x
    //   'hello
    //   x?
    // So code is ...
    // (push 'hello)
    // (copy 1)
    // (push #'cons)
    // call
    // swap
    // pop
    // ret
    lisp_object_t lambda_code = allocate_vector(7 << 4);
    lisp_object_t push = sym("push");
    svref_set(lambda_code, 0 << 4, List(push, sym("hello"))); // not happening
    svref_set(lambda_code, 1 << 4, List(sym("copy"), 1 << 4));

    struct symbol *bof = SymbolPtr(sym("cons"));

    svref_set(lambda_code, 2 << 4, List(push, bof->function));
    svref_set(lambda_code, 3 << 4, sym("call"));
    svref_set(lambda_code, 4 << 4, sym("swap"));
    svref_set(lambda_code, 5 << 4, sym("pop"));
    svref_set(lambda_code, 6 << 4, sym("ret"));

    struct symbol *symptr = SymbolPtr(sym("foo"));

    lisp_object_t fn = allocate_function();
    struct lisp_function *fnptr = LispFunctionPtr(fn);
    fnptr->kind = sym("lambda");
    fnptr->actual_function = lambda_code;
    symptr->function = fn;

    lisp_object_t calling_code = allocate_vector(3 << 4);

    svref_set(calling_code, 0 << 4, List(push, sym("snoogler")));
    svref_set(calling_code, 1 << 4, List(push, fn));
    svref_set(calling_code, 2 << 4, sym("call"));

    vm.current_code_vector = calling_code;

    vm_run(&vm);

    free_interpreter();
    vm_free(&vm);
}

int main()
{
    test_push_pop();
    test_call();
    test_function();
}