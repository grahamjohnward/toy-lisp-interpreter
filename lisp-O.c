#include "lisp.h"

#define ISTYPE(obj, type) ((((obj) & TYPE_MASK) == (type)) ? T : NIL)

lisp_object_t stringp(lisp_object_t obj)
{
    return (((obj & TYPE_MASK) == STRING_TYPE) || ((obj & TYPE_MASK) == SHORT_STRING_TYPE)) ? T : NIL;
}

lisp_object_t symbolp(lisp_object_t obj)
{
    return ISTYPE(obj, SYMBOL_TYPE);
}

lisp_object_t functionp(lisp_object_t obj)
{
    return ISTYPE(obj, FUNCTION_TYPE);
}

lisp_object_t integerp(lisp_object_t obj)
{
    uint64_t x = obj & IMMEDIATE_TYPE_MASK;
    return x == 0 ? T : NIL;
}

lisp_object_t floatp(lisp_object_t obj)
{
    return ((obj & IMMEDIATE_TYPE_MASK) == SINGLE_FLOAT_TYPE) ? T : NIL;
}

lisp_object_t consp(lisp_object_t obj)
{
    return ISTYPE(obj, CONS_TYPE);
}

lisp_object_t vectorp(lisp_object_t obj)
{
    return ISTYPE(obj, VECTOR_TYPE);
}

#undef ISTYPE

lisp_object_t function_pointer_p(lisp_object_t obj)
{
    return ((obj & IMMEDIATE_TYPE_MASK) == FUNCTION_POINTER_TYPE) ? T : NIL;
}

lisp_object_t native_pointer_p(lisp_object_t obj)
{
    return ((obj & IMMEDIATE_TYPE_MASK) == NATIVE_POINTER_TYPE) ? T : NIL;
}
