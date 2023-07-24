#ifndef LISP_H
#define LISP_H

#include <setjmp.h>
#include <stdint.h>

#include "string_buffer.h"
#include "text_stream.h"

/* This is actually just a collection of declarations that are
   required by the unit tests */

typedef uint64_t lisp_object_t;

typedef uint64_t object_header_t;

void skip_whitespace(struct text_stream *ts);
int64_t parse_integer(struct text_stream *ts);
lisp_object_t parse_string(struct text_stream *ts);
lisp_object_t parse1(struct text_stream *ts);
void parse(struct text_stream *ts, void (*callback)(void *, lisp_object_t), void *callback_data);
lisp_object_t sym(char *string);
char *read_token(struct text_stream *ts);

void init_interpreter(size_t heap_size);
void init_interpeter_from_image(char *image);
void free_interpreter();

char *print_object(lisp_object_t obj);
void print_object_to_buffer(lisp_object_t, struct string_buffer *);

#define TRACE(obj)                                                                                \
    do {                                                                                          \
        char *str = print_object(obj);                                                            \
        printf("%s:%d %s: %s (%p) = %s\n", __FILE__, __LINE__, __func__, #obj, (void *)obj, str); \
        free(str);                                                                                \
    } while (0);

void load_str(char *str);
lisp_object_t load(lisp_object_t filename);

// clang-format off
#define NIL                   0x7ffffffffffffff2
#define T                     0xfffffffffffffff2
#define TYPE_MASK             0x000000000000000f
#define PTR_MASK              0xfffffffffffffff0
#define SYMBOL_TYPE           0x0000000000000002
#define CONS_TYPE             0x0000000000000004
#define STRING_TYPE           0x0000000000000006
#define VECTOR_TYPE           0x0000000000000008
#define FUNCTION_POINTER_TYPE 0x000000000000000A
#define FUNCTION_TYPE         0x000000000000000C
#define FORWARDING_POINTER    0x0000000000000001
// clang-format on

#define ConsPtr(obj) ((struct cons *)((obj)&PTR_MASK))
#define SymbolPtr(obj) ((struct symbol *)((obj)&PTR_MASK))
#define StringPtr(obj) ((struct string_header *)((obj)&PTR_MASK))
#define VectorPtr(obj) ((struct vector *)((obj)&PTR_MASK))
#define FunctionPtr(obj) ((void (*)())((((obj)&PTR_MASK) >> 4)))
#define LispFunctionPtr(obj) ((struct lisp_function *)((obj)&PTR_MASK))

lisp_object_t svref(lisp_object_t vector, size_t index);
lisp_object_t svref_set(lisp_object_t vector, size_t index, lisp_object_t newvalue);

lisp_object_t allocate_string(size_t len, char *str);
lisp_object_t allocate_vector(size_t size);

void get_string_parts(lisp_object_t string, size_t *lenptr, char **strptr);

lisp_object_t symbolp(lisp_object_t obj);
lisp_object_t integerp(lisp_object_t obj);
lisp_object_t consp(lisp_object_t obj);
lisp_object_t stringp(lisp_object_t obj);
lisp_object_t vectorp(lisp_object_t obj);
lisp_object_t function_pointer_p(lisp_object_t obj);
lisp_object_t functionp(lisp_object_t obj);
lisp_object_t atom(lisp_object_t obj);
lisp_object_t cons(lisp_object_t car, lisp_object_t cdr);
lisp_object_t car(lisp_object_t obj);
lisp_object_t cdr(lisp_object_t obj);
lisp_object_t caar(lisp_object_t obj);
lisp_object_t cadr(lisp_object_t obj);
lisp_object_t cdar(lisp_object_t obj);
lisp_object_t cddr(lisp_object_t obj);
lisp_object_t caddr(lisp_object_t obj);
lisp_object_t cadar(lisp_object_t obj);
lisp_object_t rplaca(lisp_object_t the_cons, lisp_object_t the_car);
lisp_object_t rplacd(lisp_object_t the_cons, lisp_object_t the_cdr);
lisp_object_t string_equalp(lisp_object_t s1, lisp_object_t s2);
lisp_object_t eq(lisp_object_t o1, lisp_object_t o2);
lisp_object_t sublis(lisp_object_t a, lisp_object_t y);
lisp_object_t null(lisp_object_t obj);
lisp_object_t append(lisp_object_t x, lisp_object_t y);
lisp_object_t member(lisp_object_t x, lisp_object_t y);
lisp_object_t assoc(lisp_object_t x, lisp_object_t a);
lisp_object_t evalquote(lisp_object_t fn, lisp_object_t x);
lisp_object_t eval_toplevel(lisp_object_t e);
lisp_object_t eval(lisp_object_t e, lisp_object_t a);
lisp_object_t apply(lisp_object_t fn, lisp_object_t x, lisp_object_t a);
lisp_object_t lisp_read();
lisp_object_t print(lisp_object_t obj);
lisp_object_t princ(lisp_object_t obj);
lisp_object_t plus(lisp_object_t x, lisp_object_t y);
lisp_object_t minus(lisp_object_t x, lisp_object_t y);
lisp_object_t times(lisp_object_t x, lisp_object_t y);
lisp_object_t divide(lisp_object_t x, lisp_object_t y);
lisp_object_t raise(lisp_object_t sym, lisp_object_t value);
lisp_object_t getprop(lisp_object_t sym, lisp_object_t ind);
lisp_object_t putprop(lisp_object_t sym, lisp_object_t ind, lisp_object_t value);
lisp_object_t macroexpand1(lisp_object_t expr, lisp_object_t env);
lisp_object_t macroexpand(lisp_object_t expr, lisp_object_t env);
lisp_object_t macroexpand_all(lisp_object_t expr);
lisp_object_t save_image(lisp_object_t name);
lisp_object_t type_of(lisp_object_t obj);
lisp_object_t gensym();
lisp_object_t compile_toplevel(lisp_object_t expr);

struct syms {
    lisp_object_t lambda, quote, cond, defun, built_in_function, progn, tagbody, set, go, return_, amprest, ampbody, ampoptional, condition_case, defmacro, quasiquote, unquote, unquote_splice, let, integer, symbol, cons, string, vector, macro, function, funcall, block, pctblock, return_from;
};

struct cons {
    object_header_t header;
    lisp_object_t car;
    lisp_object_t cdr;
    uint64_t padding;
};

/* String storage is one of these immediately followed by the
 * null-terminated string */
struct string_header {
    object_header_t header;
    size_t allocated_length;
    size_t string_length;
    uint64_t padding;
};

struct lisp_function {
    object_header_t header;
    lisp_object_t kind;
    lisp_object_t actual_function;
    uint64_t padding;
};

struct symbol {
    object_header_t header;
    lisp_object_t name;
    lisp_object_t value;
    lisp_object_t function;
    lisp_object_t plist;
    uint64_t padding;
};

#define LISP_HEAP_BASE 0x400000000000

struct lisp_heap {
    size_t size_bytes;
    char *heap;
    char *freeptr;
    /* These are flipped after a GC */
    char *from_space;
    char *to_space;
};

void *get_rbp(int n);

void lisp_heap_init(struct lisp_heap *heap, size_t bytes);
void lisp_heap_free(struct lisp_heap *heap);
void gc_copy(struct lisp_heap *heap, lisp_object_t *p);

lisp_object_t list(lisp_object_t first, ...);

#define List(...) list(__VA_ARGS__, NIL)

struct return_context {
    lisp_object_t type;
    jmp_buf buf;
    lisp_object_t return_value;
    struct return_context *next;
    /* tagbody_forms is here so it can be freed in pop_return_context() */
    /* - it is not actually accessed: */
    lisp_object_t *tagbody_forms;
    size_t tagbody_forms_len;
};

struct lisp_interpreter {
    struct syms syms;
    lisp_object_t environ;
    lisp_object_t symbol_table; /* A root for GC */
    /* Machinery for returning from prog */
    struct return_context *prog_return_stack;
    /* New improved heap */
    struct lisp_heap heap;
    lisp_object_t *top_of_stack;
};

extern struct lisp_interpreter *interp;

#endif
