#include <assert.h>

#include "lisp.h"
#include "lexical_scope.h"

void lexical_context_init(struct lexical_context *ctxt)
{
    ctxt->block_alist = NIL;
    ctxt->next_block_number = 0;
    ctxt->bindings = NIL;
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

lisp_object_t lexical_context_lookup(struct lexical_context *ctxt, lisp_object_t sym)
{
    assert(symbolp(sym) != NIL);
    int i = 0;
    for (lisp_object_t bindings = ctxt->bindings; bindings != NIL; bindings = cdr(bindings)) {
	int j = 0;
	for (lisp_object_t varlist = car(bindings); varlist != NIL; varlist = cdr(varlist)) {
	    if (eq(car(varlist), sym) != NIL)
		return cons(i << 4, j << 4);
	    j++;
	}
	i++;
    }
    return NIL;
}
