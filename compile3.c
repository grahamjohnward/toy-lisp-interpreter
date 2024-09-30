#include "interp.h"
#include "lexical_scope.h"
#include "lisp.h"
#include "vm.h"

#include <assert.h>
#include <stdio.h>

lisp_object_t compile3(lisp_object_t expr, struct lexical_context *ctxt);

static lisp_object_t list_to_vector(lisp_object_t list)
{
    lisp_object_t len = length(list);
    lisp_object_t vector = allocate_vector(len);
    lisp_object_t i = 0;
    for (lisp_object_t remaining = list; remaining != NIL; remaining = cdr(remaining)) {
        svref_set(vector, i, car(remaining));
        i += 16;
    }
    return vector;
}

lisp_object_t compile3_lambda(lisp_object_t expr, struct lexical_context *ctxt)
{
    assert(car(expr) == interp->syms.lambda);
    lisp_object_t arglist = cadr(expr);
    lisp_object_t body = cddr(expr);
    lisp_object_t effective_arglist = append(arglist, cons(interp->syms.provided_arg_count, NIL));
    lexical_context_enter_scope(ctxt, effective_arglist);
    lisp_object_t compiled_body = compile3(cons(interp->syms.progn, body), ctxt);
    lexical_context_leave_scope(ctxt);
    lisp_object_t len = length(compiled_body);
    lisp_object_t code = list_to_vector(compiled_body);
    lisp_object_t result = List(
        interp->syms.push, code,
        interp->syms.push, 1 << 4,
        interp->syms.push, interp->syms.vm_make_function,
        interp->syms.call);
    return result;
}

lisp_object_t compile3_list(lisp_object_t forms, struct lexical_context *ctxt)
{
    if (forms == NIL)
        return NIL;
    return append(compile3(car(forms), ctxt), compile3_list(cdr(forms), ctxt));
}

lisp_object_t compile3_progn(lisp_object_t expr, struct lexical_context *ctxt)
{
    assert(car(expr) == interp->syms.progn);
    return compile3_list(cdr(expr), ctxt);
}

lisp_object_t compile3(lisp_object_t expr, struct lexical_context *ctxt)
{
    TRACE(expr);
    if (atom(expr) != NIL) {
        // needs to handle quote
        if (stringp(expr) != NIL || integerp(expr) != NIL) {
            return List(interp->syms.push, expr);
        } else {
            lisp_object_t thing = lexical_context_lookup(ctxt, expr);
            return List(interp->syms.copy2, thing);
        }
    } else if (symbolp(car(expr)) != NIL) {
        lisp_object_t symbol = car(expr);
        if (symbol == interp->syms.function) {
            lisp_object_t function = cadr(expr);
            if (symbolp(function) != NIL)
                abort();
            else
                return compile3_lambda(function, ctxt);
        } else if (symbol == interp->syms.progn) {
            return compile3_progn(expr, ctxt);
        } else {
            /* Compile the arguments - first to last (obviously) */
            lisp_object_t result = NIL;
            int arg_count = 0;
            for (lisp_object_t args = cdr(expr); args != NIL; args = cdr(args)) {
                lisp_object_t arg = car(args);
                result = append(result, compile3(arg, ctxt));
                arg_count++;
            }
            /* Provided argument count is the last argument */
            lisp_object_t pass_arg_count = List(interp->syms.push, arg_count << 4);
            /* Look up the symbol function at runtime */
            lisp_object_t more_stuff = List(interp->syms.push, symbol, interp->syms.call);
            result = append(append(result, pass_arg_count), more_stuff);
            return result;
        }
    } else {
        return raise(sym("bad-expression"), expr);
    }
}

lisp_object_t compile3_toplevel(lisp_object_t expr)
{
    struct lexical_context ctxt;
    lexical_context_init(&ctxt);
    return list_to_vector(compile3(expr, &ctxt));
}
