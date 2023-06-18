#include "lisp.h"

struct lexical_context {
};

static void lexical_context_init(struct lexical_context *ctxt)
{
}

static lisp_object_t compile(lisp_object_t expr, struct lexical_context *ctxt)
{
    if (consp(expr) != NIL) {
    } else {
        // With lexical scope we will do something non-trivial here
        return expr;
    }
}

lisp_object_t compile_toplevel(lisp_object_t expr)
{
    struct lexical_context ctxt;
    lexical_context_init(&ctxt);
    return compile(expr, &ctxt);
}