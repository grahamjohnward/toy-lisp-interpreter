#include "interp.h"
#include "lexical_scope.h"
#include "lisp.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

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

static lisp_object_t compile_let(lisp_object_t expr, struct lexical_context *ctxt)
{
    lisp_object_t result2 = NIL;
    lisp_object_t bindings = NIL;
    lisp_object_t varform = NIL, body = NIL, varlist = NIL, compiled_varlist = NIL;
    lisp_object_t thing = NIL;
    lisp_object_t foo = NIL, bar = NIL;
    varlist = cadr(expr);
    body = cddr(expr);
    compiled_varlist = compile_let_varlist(varlist, ctxt);

    thing = compile_list(body, ctxt);
    result2 = cons(interp->syms.let, cons(compiled_varlist, thing));

    return result2;
}

static lisp_object_t compile_quasiquote_list(lisp_object_t expr, struct lexical_context *ctxt, int depth);

static lisp_object_t compile_quasiquote(lisp_object_t expr, struct lexical_context *ctxt, int depth)
{
    if (consp(expr) == NIL) {
        return expr;
    } else if (symbolp(car(expr)) != NIL) {
        lisp_object_t symbol = car(expr);
        if (symbol == interp->syms.unquote) {
            if (depth == 0)
                return List(interp->syms.unquote, compile(cadr(expr), ctxt));
            else
                return List(interp->syms.unquote, compile_quasiquote(cadr(expr), ctxt, depth - 1));
        } else if (symbol == interp->syms.quasiquote) {
            return List(interp->syms.quasiquote, compile_quasiquote(cadr(expr), ctxt, depth + 1));
        } else {
            return cons(symbol, compile_quasiquote_list(cdr(expr), ctxt, depth));
        }
    } else {
        return compile_quasiquote_list(expr, ctxt, depth);
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

static lisp_object_t compile_if(lisp_object_t expr, struct lexical_context *ctxt)
{
    lisp_object_t test_form = cadr(expr);
    lisp_object_t then_form = caddr(expr);
    lisp_object_t else_form = cadr(cddr(expr));
    return List(interp->syms.if_, compile(test_form, ctxt), compile(then_form, ctxt), compile(else_form, ctxt));
}

static lisp_object_t compile_block(lisp_object_t expr, struct lexical_context *ctxt)
{
    lisp_object_t block_name = cadr(expr);
    lisp_object_t block_number = lexical_context_enter_block(ctxt, block_name);
    lisp_object_t body = cddr(expr);
    lisp_object_t compiled_body = compile_list(body, ctxt);
    lisp_object_t progn = cons(interp->syms.progn, compiled_body);
    lisp_object_t result = List(interp->syms.pctblock, block_number, List(sym("raise"), block_number, progn));
    // Should we try to guarantee this clean-up happens?
    // Maybe not needed since it will bail the entire compilation?
    lexical_context_leave_block(ctxt, block_name);
    return result;
}

static lisp_object_t compile_lambda(lisp_object_t expr, struct lexical_context *ctxt)
{
    assert(car(expr) == interp->syms.lambda);
    lisp_object_t arglist = cadr(expr);
    lisp_object_t body = cddr(expr);
    lisp_object_t result = List(interp->syms.function, cons(interp->syms.lambda, cons(arglist, compile_list(body, ctxt))));
    return result;
}

static lisp_object_t compile_condition_case_exception_clauses(lisp_object_t expr, struct lexical_context *ctxt, lisp_object_t exc)
{
    if (expr == NIL)
        return NIL;
    lisp_object_t first_clause = car(expr);
    lisp_object_t exception_type = car(first_clause);
    lisp_object_t code = cadr(first_clause);
    lisp_object_t compiled_code = compile(code, ctxt);
    return cons(List(exception_type, compiled_code), compile_condition_case_exception_clauses(cdr(expr), ctxt, exc));
}

static lisp_object_t compile_condition_case(lisp_object_t expr, struct lexical_context *ctxt)
{
    lisp_object_t exc = cadr(expr);
    lisp_object_t body = caddr(expr);
    lisp_object_t clauses = cdr(cddr(expr));
    lisp_object_t compiled_body = compile(body, ctxt);
    // Could imagine putting exc in the context and popping it off after?
    return cons(interp->syms.condition_case, cons(exc, cons(compiled_body, compile_condition_case_exception_clauses(clauses, ctxt, exc))));
}

static lisp_object_t compile(lisp_object_t expr, struct lexical_context *ctxt)
{
    if (atom(expr) != NIL) {
        return expr;
    } else if (symbolp(car(expr)) != NIL) {
        lisp_object_t symbol = car(expr);
        if (symbol == interp->syms.block) {
            return compile_block(expr, ctxt);
        } else if (symbol == interp->syms.return_from) {
            lisp_object_t block_name = cadr(expr);
            lisp_object_t x = assoc(block_name, ctxt->block_alist);
            if (x == NIL)
                return raise(sym("return-for-unknown-block"), block_name);
            else
                return List(sym("raise"), cdr(x), compile(caddr(expr), ctxt));
        } else if (symbol == interp->syms.quote) {
            return expr;
        } else if (symbol == interp->syms.quasiquote) {
            return List(interp->syms.quasiquote, compile_quasiquote(cadr(expr), ctxt, 0));
        } else if (symbol == interp->syms.unquote) {
            return raise(sym("runtime-error"), sym("comma-not-inside-backquote"));
        } else if (symbol == interp->syms.if_) {
            return compile_if(expr, ctxt);
        } else if (symbol == interp->syms.let) {
            return compile_let(expr, ctxt);
        } else if (symbol == interp->syms.set) {
            return List(interp->syms.set, cadr(expr), compile(car(cddr(expr)), ctxt));
        } else if (symbol == interp->syms.progn) {
            return cons(interp->syms.progn, compile_list(cdr(expr), ctxt));
        } else if (symbol == interp->syms.tagbody) {
            return cons(interp->syms.tagbody, compile_tagbody(cdr(expr), ctxt));
        } else if (symbol == interp->syms.go) {
            // Nothing to do here
            return expr;
        } else if (symbol == interp->syms.condition_case) {
            return compile_condition_case(expr, ctxt);
        } else if (symbol == interp->syms.function) {
            lisp_object_t function = cadr(expr);
            if (symbolp(function) != NIL)
                return expr;
            else
                return compile_lambda(function, ctxt);
        } else {
            return cons(car(expr), compile_list(cdr(expr), ctxt));
        }
    } else {
        return raise(sym("bad-expression"), expr);
    }
}

lisp_object_t compile_toplevel(lisp_object_t expr)
{
    struct lexical_context ctxt;
    lexical_context_init(&ctxt);
    return compile(expr, &ctxt);
}