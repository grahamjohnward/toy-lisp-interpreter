#include "lisp.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

struct lexical_context {
};

static void lexical_context_init(struct lexical_context *ctxt)
{
}

static lisp_object_t compile(lisp_object_t, struct lexical_context *ctxt);

static lisp_object_t compile_list(lisp_object_t list, struct lexical_context *ctxt)
{
    if (list == NIL)
        return NIL;
    else {
        TRACE(list);
        lisp_object_t result = cons(compile(car(list), ctxt), compile_list(cdr(list), ctxt));
        TRACE(result);
        return result;
    }
}

static lisp_object_t compile_let_varlist(lisp_object_t expr, struct lexical_context *ctxt)
{
    return expr;
}

static lisp_object_t compile_let(lisp_object_t expr, struct lexical_context *ctxt)
{
    lisp_object_t varlist = cadr(expr);
    lisp_object_t body = cddr(expr);
    return cons(interp->syms.let, cons(compile_let_varlist(varlist, ctxt), compile_list(body, ctxt)));
}

static lisp_object_t compile_quasiquote_list(lisp_object_t expr, struct lexical_context *ctxt, int depth);

static lisp_object_t compile_quasiquote(lisp_object_t expr, struct lexical_context *ctxt, int depth)
{
    TRACE(expr);
    if (consp(expr) == NIL) {
        return expr;
    } else {
        if (symbolp(car(expr)) != NIL) {
            lisp_object_t symbol = car(expr);
            if (symbol == interp->syms.unquote) {
                lisp_object_t arg = cadr(expr);
                if (depth == 0)
                    return cons(interp->syms.unquote, cons(compile(arg, ctxt), NIL));
                else
                    return cons(interp->syms.unquote, cons(compile_quasiquote(arg, ctxt, depth - 1), NIL));
            } else if (symbol == interp->syms.unquote_splice) {
                lisp_object_t arg = cadr(expr);
                if (depth == 0)
                    return cons(interp->syms.unquote_splice, cons(compile(arg, ctxt), NIL));
                else
                    return cons(interp->syms.unquote_splice, cons(compile_quasiquote(arg, ctxt, depth - 1), NIL));
            } else if (symbol == interp->syms.quasiquote) {
                TRACE(cadr(expr));
                return cons(interp->syms.quasiquote, cons(compile_quasiquote(cadr(expr), ctxt, depth + 1), NIL));
            } else {
                TRACE(car(expr));
                lisp_object_t bof = cons(interp->syms.quasiquote, cons(car(expr), compile_quasiquote_list(cdr(expr), ctxt, depth)));
                TRACE(bof);
                return bof;
            }
        } else {
            return expr;
        }
    }
}

static lisp_object_t compile_quasiquote_list(lisp_object_t expr, struct lexical_context *ctxt, int depth)
{
    TRACE(expr);
    if (expr == NIL) {
        return NIL;
    } else {
        lisp_object_t thing1 = compile_quasiquote(car(expr), ctxt, depth);
        TRACE(thing1);
        lisp_object_t result = cons(thing1, compile_quasiquote_list(cdr(expr), ctxt, depth));
        TRACE(result);
        return result;
    }
}

static lisp_object_t compile_tagbody(lisp_object_t expr, struct lexical_context *ctxt)
{
    if (expr == NIL)
        return NIL;
    else if (symbolp(car(expr)) != NIL)
        return cons(car(expr), compile_tagbody(cdr(expr), ctxt));
    else
        return cons(compile(car(expr), ctxt), compile_tagbody(cdr(expr), ctxt));
}

static lisp_object_t compile(lisp_object_t expr, struct lexical_context *ctxt)
{
    if (consp(expr) != NIL) {
        if (symbolp(car(expr)) != NIL) {
            lisp_object_t symbol = car(expr);
            if (symbol == interp->syms.quote) {
                return expr;
            } else if (symbol == interp->syms.quasiquote) {
                return cons(interp->syms.quasiquote, cons(compile_quasiquote(cadr(expr), ctxt, 0), NIL));
            } else if (symbol == interp->syms.unquote) {
                return raise(sym("runtime-error"), sym("comma-not-inside-backquote"));
            } else if (symbol == interp->syms.cond) {
                // TODO
                // basically easy
                return expr;
            } else if (symbol == interp->syms.let) {
                return compile_let(expr, ctxt);
            } else if (symbol == interp->syms.defun) {
                lisp_object_t name = cadr(expr);
                lisp_object_t arglist = car(cddr(expr));
                lisp_object_t body = cdr(cddr((expr)));
                return cons(interp->syms.defun, cons(name, cons(arglist, compile_list(body, ctxt))));
            } else if (symbol == interp->syms.defmacro) {
                // TODO
                // this just defines a function and sets a special property on the symbol,
                // so there is no weirdness involved
                return expr;
            } else if (symbol == interp->syms.set) {
                return cons(interp->syms.set, cons(cadr(expr), cons(compile(car(cddr(expr)), ctxt), NIL)));
            } else if (symbol == interp->syms.prog) {
                // TODO
                return expr;
            } else if (symbol == interp->syms.progn) {
                return cons(interp->syms.progn, compile_list(cdr(expr), ctxt));
            } else if (symbol == interp->syms.tagbody) {
                return cons(interp->syms.tagbody, compile_tagbody(cdr(expr), ctxt));
            } else if (symbol == interp->syms.go) {
                // Nothing to do here
                return expr;
            } else if (symbol == interp->syms.return_) {
                // Nothing to do here for now
                // Perhaps we could convert it to (raise 'return)
                return expr;
            } else if (symbol == interp->syms.condition_case) {
                // TODO
                return expr;
            } else if (symbol == interp->syms.function) {
                // TODO
                // we would need to compile any lambda
                return expr;
            } else {
                return cons(car(expr), compile_list(cdr(expr), ctxt));
            }
        } else {
            return raise(sym("bad-expression"), expr);
        }
    } else {
        // With lexical scope we will do something non-trivial here
        return expr;
    }
}

lisp_object_t compile_toplevel(lisp_object_t expr)
{
    struct lexical_context ctxt;
    TRACE(expr);
    lexical_context_init(&ctxt);
    lisp_object_t compiled = compile(expr, &ctxt);
    TRACE(compiled);
    return compiled;
}