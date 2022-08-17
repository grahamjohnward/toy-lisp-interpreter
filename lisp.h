#ifndef LISP_H
#define LISP_H

#include <stdint.h>

#include "string_buffer.h"

/* This is actually just a collection of declarations that are
   required by the unit tests */

typedef uint64_t lisp_object_t;

void skip_whitespace(char** text);
int parse_integer(char** text);
lisp_object_t parse_string(char**);
lisp_object_t parse1(char**);
void parse(char* text, void (*callback)(void*, lisp_object_t), void* callback_data);
lisp_object_t sym(char* string);

void init_interpreter(size_t heap_size);
void free_interpreter();

char* print_object(lisp_object_t obj);
void print_object_to_buffer(lisp_object_t, struct string_buffer*);

void load_str(char *str);

#define NIL 0xfffffffffffffff9
#define T 0xfffffffffffffff1

#define TYPE_MASK 0x7
#define PTR_MASK 0xfffffffffffffff8
#define INTEGER_TYPE 0
#define SYMBOL_TYPE 1
#define CONS_TYPE 2
#define STRING_TYPE 3
#define VECTOR_TYPE 4

#define ConsPtr(obj) ((struct cons*)((obj)&PTR_MASK))
#define SymbolPtr(obj) ((struct symbol*)((obj)&PTR_MASK))
/* StringPtr is different as a string is not a struct */
#define StringPtr(obj) ((size_t*)((obj)&PTR_MASK))
#define VectorPtr(obj) ((struct vector*)((obj)&PTR_MASK))

lisp_object_t svref(lisp_object_t vector, size_t index);
lisp_object_t svref_set(lisp_object_t vector, size_t index, lisp_object_t newvalue);
    
lisp_object_t allocate_string(size_t len, char* str);
lisp_object_t allocate_vector(size_t size);

void get_string_parts(lisp_object_t string, size_t* lenptr, char** strptr);

lisp_object_t symbolp(lisp_object_t obj);
lisp_object_t integerp(lisp_object_t obj);
lisp_object_t consp(lisp_object_t obj);
lisp_object_t stringp(lisp_object_t obj);
lisp_object_t vectorp(lisp_object_t obj);
lisp_object_t cons(lisp_object_t car, lisp_object_t cdr);
lisp_object_t car(lisp_object_t obj);
lisp_object_t cdr(lisp_object_t obj);
lisp_object_t string_equalp(lisp_object_t s1, lisp_object_t s2);
lisp_object_t eq(lisp_object_t o1, lisp_object_t o2);
lisp_object_t sublis(lisp_object_t a, lisp_object_t y);
lisp_object_t null(lisp_object_t obj);
lisp_object_t append(lisp_object_t x, lisp_object_t y);
lisp_object_t member(lisp_object_t x, lisp_object_t y);    
lisp_object_t assoc(lisp_object_t x, lisp_object_t a);
lisp_object_t pairlis(lisp_object_t x, lisp_object_t y, lisp_object_t a);
lisp_object_t evalquote(lisp_object_t fn, lisp_object_t x);
lisp_object_t eval_toplevel(lisp_object_t e);
lisp_object_t eval(lisp_object_t e, lisp_object_t a);

struct syms {
    lisp_object_t car, cdr, cons, atom, eq, lambda, label, quote, cond, defun, load;
};

/* Might be nice to have a lisp_memory struct separate from the interpreter */
struct lisp_interpreter {
    lisp_object_t* heap;
    lisp_object_t* next_free;
    lisp_object_t symbol_table; /* A root for GC */
    size_t heap_size_bytes;
    struct syms syms;
    lisp_object_t environ;
};

extern struct lisp_interpreter *interp;

#endif
