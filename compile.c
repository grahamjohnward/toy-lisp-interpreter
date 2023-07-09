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
    else
        return cons(compile(car(list), ctxt), compile_list(cdr(list), ctxt));
}

static lisp_object_t compile_let_varlist(lisp_object_t expr, struct lexical_context *ctxt)
{
    if (expr == NIL) {
        return NIL;
    } else {
        lisp_object_t first = car(expr);
        if (consp(first) != NIL)
            return cons(List(car(first), compile(cadr(first), ctxt)), compile_let_varlist(cdr(expr), ctxt));
        else
            return cons(first, compile_let_varlist(cdr(expr), ctxt));
    }
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
    if (consp(expr) == NIL) {
        return expr;
    } else {
        if (symbolp(car(expr)) != NIL) {
            lisp_object_t symbol = car(expr);
            if (symbol == interp->syms.unquote) {
                if (depth == 0) {
                    return cons(interp->syms.unquote, cons(compile(cadr(expr), ctxt), NIL));
                } else {
                    return cons(interp->syms.unquote, cons(compile_quasiquote(cadr(expr), ctxt, depth - 1), NIL));
                }
            } else if (symbol == interp->syms.quasiquote) {
                return cons(interp->syms.quasiquote, cons(compile_quasiquote(cadr(expr), ctxt, depth + 1), NIL));
            } else {
                return cons(symbol, compile_quasiquote_list(cdr(expr), ctxt, depth));
            }
        } else {
            return compile_quasiquote_list(expr, ctxt, depth);
        }
    }
}

static lisp_object_t compile_quasiquote_list(lisp_object_t expr, struct lexical_context *ctxt, int depth)
{
    if (expr == NIL)
        return NIL;
    else
        return cons(compile_quasiquote(car(expr), ctxt, depth), compile_quasiquote_list(cdr(expr), ctxt, depth));
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

static lisp_object_t compile_cond_clauses(lisp_object_t clauses, struct lexical_context *ctxt)
{
    if (clauses == NIL) {
        return NIL;
    } else {
        lisp_object_t first_clause = car(clauses);
        lisp_object_t a = car(first_clause);
        lisp_object_t b = cadr(first_clause);
        return cons(cons(compile(a, ctxt), cons(compile(b, ctxt), NIL)), compile_cond_clauses(cdr(clauses), ctxt));
    }
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
                return cons(interp->syms.cond, compile_cond_clauses(cdr(expr), ctxt));
            } else if (symbol == interp->syms.let) {
                return compile_let(expr, ctxt);
            } else if (symbol == interp->syms.defun) {
                lisp_object_t name = cadr(expr);
                lisp_object_t arglist = car(cddr(expr));
                lisp_object_t body = cdr(cddr((expr)));
                return cons(interp->syms.defun, cons(name, cons(arglist, compile_list(body, ctxt))));
            } else if (symbol == interp->syms.defmacro) {
                lisp_object_t name = cadr(expr);
                lisp_object_t arglist = car(cddr(expr));
                lisp_object_t body = cdr(cddr((expr)));
                return cons(interp->syms.defmacro, cons(name, cons(arglist, compile_list(body, ctxt))));
            } else if (symbol == interp->syms.set) {
                return cons(interp->syms.set, cons(cadr(expr), cons(compile(car(cddr(expr)), ctxt), NIL)));
            } else if (symbol == interp->syms.prog) {
                lisp_object_t varlist = cadr(expr);
                lisp_object_t body = cddr(expr);
                return cons(interp->syms.prog, cons(varlist, compile_list(body, ctxt)));
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
                lisp_object_t exc = cadr(expr);
                lisp_object_t body = caddr(expr);
                lisp_object_t clauses = cdr(cddr(expr));
                return cons(interp->syms.condition_case, cons(exc, cons(compile_list(body, ctxt), compile_let_varlist(clauses, ctxt))));
            } else if (symbol == interp->syms.function) {
                lisp_object_t function = cadr(expr);
                if (symbolp(function) != NIL) {
                    return expr;
                } else {
                    lisp_object_t arglist = cadr(function);
                    lisp_object_t body = cddr(function);
                    return cons(interp->syms.function, cons(cons(interp->syms.lambda, cons(arglist, compile_list(body, ctxt))), NIL));
                }
            } else {
                return cons(car(expr), compile_list(cdr(expr), ctxt));
            }
        } else {
            return raise(sym("bad-expression"), expr);
        }
    } else {
        // With lexical scope we will do something interesting here
        return expr;
    }
}

lisp_object_t compile_toplevel(lisp_object_t expr)
{
    struct lexical_context ctxt;
    lexical_context_init(&ctxt);
    return compile(expr, &ctxt);
}