#include "lisp.h"

struct lexical_context {
    lisp_object_t block_alist;
    lisp_object_t next_block_number;
};

void lexical_context_init(struct lexical_context *ctxt);

lisp_object_t lexical_context_enter_block(struct lexical_context *ctxt, lisp_object_t block_name);

void lexical_context_leave_block(struct lexical_context *ctxt, lisp_object_t block_name);
