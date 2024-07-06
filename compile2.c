#include "lexical_scope.h"
#include "lisp.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static lisp_object_t compile2(lisp_object_t, struct lexical_context *ctxt);

static lisp_object_t compile2_list(lisp_object_t list, struct lexical_context *ctxt)
{
    if (list == NIL)
        return NIL;
    else
        return cons(compile2(car(list), ctxt), compile2_list(cdr(list), ctxt));
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

static lisp_object_t last(lisp_object_t list)
{
    if (list == NIL)
        return NIL;
    // check_cons(list);
    if (cdr(list) == NIL)
        return list;
    else
        return last(cdr(list));
}

// XXX change for stack machine
static lisp_object_t compile2_let_varlist(lisp_object_t varlist, int *n, lisp_object_t *bindings)
{
    if (varlist == NIL)
        return NIL;
    lisp_object_t initializer = NIL;
    lisp_object_t var = NIL;
    lisp_object_t varform = car(varlist);
    if (consp(varform) != NIL) {
        var = car(varform);
        initializer = cadr(varform);
    } else {
        var = varform;
        initializer = NIL;
    }
    push(var, bindings);
    return cons(List(sym("%setv"), 0, (*n)++ << 4, initializer), compile2_let_varlist(cdr(varlist), n, bindings));
}

// XXX change for stack machine
static lisp_object_t compile2_let(lisp_object_t expr, struct lexical_context *ctxt)
{
    lisp_object_t result = NIL;
    lisp_object_t initializers_and_compiled_body = NIL;
    lisp_object_t body = NIL;
    lisp_object_t bindings = NIL;
    int varcount = 0;
    initializers_and_compiled_body = compile2_let_varlist(cadr(expr), &varcount, &bindings);
    ctxt->bindings = cons(reverse(bindings), ctxt->bindings);
    body = cddr(expr);
    rplacd(last(initializers_and_compiled_body), compile2_list(body, ctxt));
    ctxt->bindings = cdr(ctxt->bindings);
    result = cons(sym("%with-scope"), cons(varcount << 4, initializers_and_compiled_body));
    return result;
}

static lisp_object_t compile2_quasiquote_list(lisp_object_t expr, struct lexical_context *ctxt, int depth);

// XXX change for stack machine
// ??? how to compile quasiquote to stack machine???
static lisp_object_t compile2_quasiquote(lisp_object_t expr, struct lexical_context *ctxt, int depth)
{
    if (consp(expr) == NIL) {
        return expr;
    } else if (symbolp(car(expr)) != NIL) {
        lisp_object_t symbol = car(expr);
        if (symbol == interp->syms.unquote) {
            if (depth == 0)
                return List(interp->syms.unquote, compile2(cadr(expr), ctxt));
            else
                return List(interp->syms.unquote, compile2_quasiquote(cadr(expr), ctxt, depth - 1));
        } else if (symbol == interp->syms.quasiquote) {
            return List(interp->syms.quasiquote, compile2_quasiquote(cadr(expr), ctxt, depth + 1));
        } else {
            return cons(symbol, compile2_quasiquote_list(cdr(expr), ctxt, depth));
        }
    } else {
        return compile2_quasiquote_list(expr, ctxt, depth);
    }
}

static lisp_object_t compile2_quasiquote_list(lisp_object_t expr, struct lexical_context *ctxt, int depth)
{
    if (expr == NIL)
        return NIL;
    else
        return cons(compile2_quasiquote(car(expr), ctxt, depth), compile2_quasiquote_list(cdr(expr), ctxt, depth));
}

// XXX change for stack machine
static lisp_object_t compile2_tagbody(lisp_object_t expr, struct lexical_context *ctxt)
{
    if (expr == NIL)
        return NIL;
    else if (symbolp(car(expr)) != NIL)
        return cons(car(expr), compile2_tagbody(cdr(expr), ctxt));
    else
        return cons(compile2(car(expr), ctxt), compile2_tagbody(cdr(expr), ctxt));
}

// XXX change for stack machine
static lisp_object_t compile2_if(lisp_object_t expr, struct lexical_context *ctxt)
{
    lisp_object_t test_form = cadr(expr);
    lisp_object_t then_form = caddr(expr);
    lisp_object_t else_form = cadr(cddr(expr));
    return List(interp->syms.if_, compile2(test_form, ctxt), compile2(then_form, ctxt), compile2(else_form, ctxt));
}

static lisp_object_t compile2_block(lisp_object_t expr, struct lexical_context *ctxt)
{
    lisp_object_t block_name = cadr(expr);
    lisp_object_t block_number = lexical_context_enter_block(ctxt, block_name);
    lisp_object_t body = cddr(expr);
    lisp_object_t compiled_body = compile2_list(body, ctxt);
    lisp_object_t progn = cons(interp->syms.progn, compiled_body);
    lisp_object_t result = List(interp->syms.pctblock, block_number, List(sym("raise"), block_number, progn));
    // Should we try to guarantee this clean-up happens?
    // Maybe not needed since it will bail the entire compilation?
    lexical_context_leave_block(ctxt, block_name);
    return result;
}

static lisp_object_t compile2_lambda(lisp_object_t expr, struct lexical_context *ctxt)
{
    // What this needs is a surefire way to detect closures (it is easy)
    assert(car(expr) == interp->syms.lambda);
    lisp_object_t arglist = cadr(expr);
    lisp_object_t body = cddr(expr);
    ctxt->bindings = cons(arglist, ctxt->bindings);
    // Probably we shouldn't simply pass through the arglist - we should
    // compile to something like %lambda which just takes arity
    // See pairlis2 for what we currently support by way of arglists
    // Basically it's &rest, &body (these are synonymous), and &optional
    lisp_object_t compiled_body = List(sym("%set-return-value"), cons(interp->syms.progn, compile2_list(body, ctxt)));
    lisp_object_t result = List(interp->syms.function, List(interp->syms.lambda, arglist, compiled_body));
    ctxt->bindings = cdr(ctxt->bindings);
    return result;
}

static lisp_object_t compile2_condition_case_exception_clauses(lisp_object_t expr, struct lexical_context *ctxt, lisp_object_t exc)
{
    if (expr == NIL)
        return NIL;
    lisp_object_t first_clause = car(expr);
    lisp_object_t exception_type = car(first_clause);
    lisp_object_t code = cadr(first_clause);
    ctxt->bindings = cons(List(exc), ctxt->bindings);
    lisp_object_t compiled_code = compile2(code, ctxt);
    ctxt->bindings = cdr(ctxt->bindings);
    return cons(List(exception_type, compiled_code), compile2_condition_case_exception_clauses(cdr(expr), ctxt, exc));
}

static lisp_object_t compile2_condition_case(lisp_object_t expr, struct lexical_context *ctxt)
{
    lisp_object_t exc = cadr(expr);
    lisp_object_t body = caddr(expr);
    lisp_object_t clauses = cdr(cddr(expr));
    lisp_object_t compiled_body = compile2(body, ctxt);
    // Could imagine putting exc in the context and popping it off after?
    return cons(interp->syms.condition_case, cons(exc, cons(compiled_body, compile2_condition_case_exception_clauses(clauses, ctxt, exc))));
}

static lisp_object_t compile2(lisp_object_t expr, struct lexical_context *ctxt)
{
    if (atom(expr) != NIL) {
        if (expr != NIL && expr != T && symbolp(expr) != NIL) {
            lisp_object_t lookup_result = lexical_context_lookup(ctxt, expr);
            if (lookup_result == NIL) {
                char *str = print_object(expr);
                // Could check symbol value slot here before warning
                printf("Oh no unbound variable: %s\n", str);
                free(str);
            } else {
                // lisp_object_t thing = List(expr, lookup(ctxt, expr));
                // TRACE(thing);
                return List(sym("%v"), car(lookup_result), cdr(lookup_result));
            }
        }
        return expr;
    } else if (symbolp(car(expr)) != NIL) {
        lisp_object_t symbol = car(expr);
        if (symbol == interp->syms.block) {
            return compile2_block(expr, ctxt);
        } else if (symbol == interp->syms.return_from) {
            lisp_object_t block_name = cadr(expr);
            lisp_object_t x = assoc(block_name, ctxt->block_alist);
            if (x == NIL)
                return raise(sym("return-for-unknown-block"), block_name);
            else
                return List(sym("raise"), cdr(x), compile2(caddr(expr), ctxt));
        } else if (symbol == interp->syms.quote) {
            return expr;
        } else if (symbol == interp->syms.quasiquote) {
            return List(interp->syms.quasiquote, compile2_quasiquote(cadr(expr), ctxt, 0));
        } else if (symbol == interp->syms.unquote) {
            return raise(sym("runtime-error"), sym("comma-not-inside-backquote"));
        } else if (symbol == interp->syms.if_) {
            return compile2_if(expr, ctxt);
        } else if (symbol == interp->syms.let) {
            return compile2_let(expr, ctxt);
        } else if (symbol == interp->syms.set) {
            return List(interp->syms.set, cadr(expr), compile2(car(cddr(expr)), ctxt));
        } else if (symbol == interp->syms.progn) {
            return cons(interp->syms.progn, compile2_list(cdr(expr), ctxt));
        } else if (symbol == interp->syms.tagbody) {
            return cons(interp->syms.tagbody, compile2_tagbody(cdr(expr), ctxt));
        } else if (symbol == interp->syms.go) {
            // Nothing to do here
            return expr;
        } else if (symbol == interp->syms.condition_case) {
            return compile2_condition_case(expr, ctxt);
        } else if (symbol == interp->syms.function) {
            lisp_object_t function = cadr(expr);
            if (symbolp(function) != NIL)
                return expr;
            else
                return compile2_lambda(function, ctxt);
        } else {
            // Looks like a function call.
            // All function calls are compiled to a runtime lookup of the symbol's
            // function value.  Then followed by a runtime call to funcall?
            // That sounds kind of suboptimal
            return cons(car(expr), compile2_list(cdr(expr), ctxt));
        }
    } else {
        return raise(sym("bad-expression"), expr);
    }
}

lisp_object_t compile2_toplevel(lisp_object_t expr)
{
    struct lexical_context ctxt;
    lexical_context_init(&ctxt);
    return compile2(expr, &ctxt);
}