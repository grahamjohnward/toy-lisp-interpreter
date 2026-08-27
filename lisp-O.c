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

void check_cons(lisp_object_t obj)
{
    if (consp(obj) == NIL)
        raise(sym("type-error"), cons(sym("cons"), obj));
}

lisp_object_t car(lisp_object_t obj)
{
    if (obj == NIL)
        return NIL;
    check_cons(obj);
    return ConsPtr(obj)->car;
}

lisp_object_t cdr(lisp_object_t obj)
{
    if (obj == NIL)
        return NIL;
    check_cons(obj);
    return ConsPtr(obj)->cdr;
}

lisp_object_t rplaca(lisp_object_t the_cons, lisp_object_t the_car)
{
    check_cons(the_cons);
    struct cons *p = ConsPtr(the_cons);
    p->car = the_car;
    return the_cons;
}

lisp_object_t rplacd(lisp_object_t the_cons, lisp_object_t the_cdr)
{
    check_cons(the_cons);
    struct cons *p = ConsPtr(the_cons);
    p->cdr = the_cdr;
    return the_cons;
}

lisp_object_t eq(lisp_object_t o1, lisp_object_t o2)
{
    return o1 == o2 ? T : NIL;
}
