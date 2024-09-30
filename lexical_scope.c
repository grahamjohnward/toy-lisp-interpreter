
#include "lexical_scope.h"
#include "lisp.h"

#include "interp.h"

#include <assert.h>
#include <stdio.h>

void lexical_context_init(struct lexical_context *ctxt)
{
    ctxt->block_alist = NIL;
    ctxt->next_block_number = 0;
    ctxt->bindings = NIL;
    ctxt->n_bindings = 0;
}

lisp_object_t lexical_context_enter_block(struct lexical_context *ctxt, lisp_object_t block_name)
{
    struct symbol *s = SymbolPtr(interp->syms.pctblock);
    lisp_object_t block_number = NIL;
    if (s->value == NIL)
        s->value = 0;
    block_number = s->value;
    ctxt->block_alist = cons(cons(block_name, block_number), ctxt->block_alist);
    s->value += 16;
    return block_number;
}

void lexical_context_leave_block(struct lexical_context *ctxt, lisp_object_t block_name)
{
    lisp_object_t head = car(ctxt->block_alist);
    assert(head != NIL);
    assert(eq(car(head), block_name) != NIL);
    ctxt->block_alist = cdr(ctxt->block_alist);
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

void lexical_context_enter_scope(struct lexical_context *ctxt, lisp_object_t bindings)
{
    ctxt->bindings = cons(bindings, ctxt->bindings);
    lisp_object_t len = length(bindings);
    ctxt->n_bindings += len;
    TRACE(ctxt->bindings);
}

void lexical_context_leave_scope(struct lexical_context *ctxt)
{
    ctxt->n_bindings -= length(car(ctxt->bindings));
    ctxt->bindings = cdr(ctxt->bindings);
}

lisp_object_t lexical_context_lookup(struct lexical_context *ctxt, lisp_object_t symbol)
{
    lisp_object_t bindings = ctxt->bindings;
    lisp_object_t i = ctxt->n_bindings;
    while (bindings != NIL) {
        lisp_object_t bindings_one_scope = car(bindings);
        while (bindings_one_scope != NIL) {
            if (eq(car(bindings_one_scope), symbol) != NIL) {
                return i;
            }
            i -= 16;
            bindings_one_scope = cdr(bindings_one_scope);
        }
        bindings = cdr(bindings);
    }
    return NIL;
}
