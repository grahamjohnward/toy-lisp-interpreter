#ifndef LISP_H
#define LISP_H

#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>

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
void init_interpreter2(size_t heap_size, int use_vm);
void init_interpreter_from_image(char *image);
void free_interpreter();

char *print_object(lisp_object_t obj);
void print_object_to_buffer(lisp_object_t, struct string_buffer *);

#define TRACE(obj)                                                                                    \
    do {                                                                                              \
        if (getenv("NO_TRACING") == NULL) {                                                           \
            char *str = print_object(obj);                                                            \
            printf("%s:%d %s: %s (%p) = %s\n", __FILE__, __LINE__, __func__, #obj, (void *)obj, str); \
            free(str);                                                                                \
        }                                                                                             \
    } while (0);

void load_str(char *str);
lisp_object_t load(lisp_object_t filename);

// clang-format off
#define LISP_HEAP_BASE        0x0000400000000000

#define IMMEDIATE_TYPE_MASK   0x0000000000000007
#define IMMEDIATE_VALUE_MASK  0xfffffffffffffff8

/* Special symbols */
#define NIL                   0x0100000000000001
#define T                     0x0100fffffffffff1
#define VARARGS_LIST_SENTINEL 0x0100f00000000001

/* Immediate types */
#define OBJECT_TYPE           0x0000000000000001
#define FUNCTION_POINTER_TYPE 0x0000000000000002
#define NATIVE_POINTER_TYPE   0x0000000000000003

#define TYPE_MASK             0xff00000000000007
#define PTR_MASK              0x0000fffffffffff8

#define SYMBOL_TYPE           0x0100000000000001
#define CONS_TYPE             0x0200000000000001
#define STRING_TYPE           0x0300000000000001
#define VECTOR_TYPE           0x0400000000000001
#define FUNCTION_TYPE         0x0500000000000001

#define FORWARDING_POINTER    0x0001000000000000
// clang-format on

#define ConsPtr(obj) ((struct cons *)((obj) & PTR_MASK))
#define SymbolPtr(obj) ((struct symbol *)((obj) & PTR_MASK))
#define StringPtr(obj) ((struct string_header *)((obj) & PTR_MASK))
#define VectorPtr(obj) ((struct vector *)((obj) & PTR_MASK))
#define FunctionPtr(obj) ((void (*)())((obj) & IMMEDIATE_VALUE_MASK))
#define NativePtr(obj) ((void *)((obj) & IMMEDIATE_VALUE_MASK))
#define LispFunctionPtr(obj) ((struct lisp_function *)((obj) & PTR_MASK))

#define LispInt(x) (((uint64_t)(x)) << 3)
#define Int(x) (((int64_t)(x)) >> 3)

void check_vector(lisp_object_t obj);

lisp_object_t svref_c(lisp_object_t vector, size_t index);
lisp_object_t svref(lisp_object_t vector, lisp_object_t index);
lisp_object_t svref_set(lisp_object_t vector, lisp_object_t index, lisp_object_t newvalue);

lisp_object_t allocate_string(size_t len, char *str);
lisp_object_t allocate_vector(lisp_object_t size);
lisp_object_t allocate_function();

void get_string_parts(lisp_object_t string, size_t *lenptr, char **strptr);

lisp_object_t length(lisp_object_t seq);
int length_c(lisp_object_t seq);

lisp_object_t push(lisp_object_t obj, lisp_object_t *place);

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
lisp_object_t compile3_toplevel(lisp_object_t expr);
lisp_object_t append(lisp_object_t list1, lisp_object_t list2);
lisp_object_t symbol_value(lisp_object_t symbol);
lisp_object_t set_symbol_value(lisp_object_t symbol, lisp_object_t value);

lisp_object_t macroexpand_all_quasiquote(lisp_object_t expr, int depth);

void init_compiler();

struct cons {
    object_header_t header;
    lisp_object_t car;
    lisp_object_t cdr;
};

/* String storage is one of these immediately followed by the
 * null-terminated string */
struct string_header {
    object_header_t header;
    size_t allocated_length;
    size_t string_length;
};

struct lisp_function {
    object_header_t header;
    lisp_object_t kind;
    lisp_object_t name;
    lisp_object_t actual_function;
};

struct symbol {
    object_header_t header;
    lisp_object_t name;
    lisp_object_t value;
    lisp_object_t function;
    lisp_object_t plist;
    uint64_t hash;
};

struct lisp_heap {
    size_t size_bytes;
    char *heap;
    char *freeptr;
    /* These are flipped after a GC */
    char *from_space;
    char *to_space;
};

void *get_frame_pointer(int n);

void lisp_heap_init(struct lisp_heap *heap, size_t bytes);
void lisp_heap_free(struct lisp_heap *heap);
void gc_copy(struct lisp_heap *heap, lisp_object_t *p);

void gc_copy_jmp_buf(struct lisp_heap *heap, jmp_buf buf);

lisp_object_t list(lisp_object_t first, ...);

#define List(...) list(__VA_ARGS__, VARARGS_LIST_SENTINEL)

#endif
