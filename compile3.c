#include "lisp.h"
#include "vm.h"

// #include "lexical_scope.h"

#include <assert.h>

struct amazing {
    // We need to manage the stack of bindings (let inside lambda etc.) via lisp objects
    // in case the GC runs while we're running the compiler.  In other words we don't
    // want to use linked C structs
    //
    // So `bindings` may be some fancy thing
    // But what?
    lisp_object_t bindings;
};

static void amazing_init(struct amazing *ctxt)
{
    ctxt->bindings = NIL;
}

static lisp_object_t reverse1(lisp_object_t list, lisp_object_t aux)
{
    lisp_object_t tmp1 = NIL;
    lisp_object_t tmp2 = NIL;
    lisp_object_t tmp3 = NIL;
    if (list == NIL)
        return aux;
    tmp1 = cdr(list);
    tmp3 = car(list);
    tmp2 = cons(tmp3, aux);
    return reverse1(tmp1, tmp2);
}

static lisp_object_t reverse(lisp_object_t list)
{
    return reverse1(list, NIL);
}

lisp_object_t compile3_lambda(lisp_object_t expr, struct amazing *ctxt)
{
    assert(car(expr) == interp->syms.lambda);
    lisp_object_t arglist = cadr(expr);
    lisp_object_t body = cddr(expr);
    /*
      |<- low stack                      high stack ->|
      blah blah  arg1       arg2      (garbage/nothing)
                 ^ top - 2  ^ top -1  ^ top

So for (lambda (a b c) ...), relative to top_of_data_stack, the offsets are

  c 1
  b 2
  a 3

at the start of the execution of the lambda.  Or stack looks like:

  c
  b
  a

What if we introduce (let ((a 1) (x 2) (y 3)) ... )?  Stack now looks like

  3
  2
  1
  c
  b
  a


and a has a new binding.  Offsets are now

y 1
x 2
a 3
c 4 (unchanged binding)
b 5 (unchanged binding)

so original offsets have been changed by 3

    */
    ctxt->bindings = reverse(arglist);

    abort();
}

lisp_object_t compile3(lisp_object_t expr, struct amazing *ctxt)
{
    if (atom(expr) != NIL) {
        abort();
    } else if (symbolp(car(expr)) != NIL) {
        lisp_object_t symbol = car(expr);
        lisp_object_t function = cadr(expr);
        if (symbolp(function) != NIL)
            abort();
        // return expr;
        else
            return compile3_lambda(function, ctxt);
    } else {
        return raise(sym("bad-expression"), expr);
    }
}

lisp_object_t compile3_toplevel(lisp_object_t expr)
{
    struct amazing ctxt;
    amazing_init(&ctxt);
    return compile3(expr, &ctxt);
}