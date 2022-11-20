#include "lisp.h"
#include "string_buffer.h"
#include "text_stream.h"

#include <alloca.h>
#include <assert.h>
#include <fcntl.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>
#include <unistd.h>

struct lisp_interpreter *interp;

static int interpreter_initialized;

lisp_object_t *top_of_stack = NULL;

struct symbol {
    lisp_object_t name;
    lisp_object_t value;
    lisp_object_t function;
    lisp_object_t plist;
};

struct vector {
    size_t len;
    lisp_object_t storage;
};

lisp_object_t istype(lisp_object_t obj, uint64_t type);

static void check_type(lisp_object_t obj, uint64_t type)
{
    static char *type_names[6] = { "unused", "symbol", "cons", "string", "vector", "function pointer" };
    if (istype(obj, type) == NIL) {
        static char buf[1024];
        char *obj_string = print_object(obj);
        int len = snprintf(buf, 1024, "Not a %s: %s", type_names[type >> 60], obj_string);
        raise(sym("type-error"), allocate_string(len, buf));
    }
}

static void check_cons(lisp_object_t obj)
{
    check_type(obj, CONS_TYPE);
}

static void check_string(lisp_object_t obj)
{
    check_type(obj, STRING_TYPE);
}

static void check_symbol(lisp_object_t obj)
{
    check_type(obj, SYMBOL_TYPE);
}

static void check_vector(lisp_object_t obj)
{
    check_type(obj, VECTOR_TYPE);
}

static void check_function_pointer(lisp_object_t obj)
{
    check_type(obj, FUNCTION_POINTER_TYPE);
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

lisp_object_t istype(lisp_object_t obj, uint64_t type)
{
    return (obj & TYPE_MASK) == type ? T : NIL;
}

lisp_object_t stringp(lisp_object_t obj)
{
    return istype(obj, STRING_TYPE);
}

lisp_object_t symbolp(lisp_object_t obj)
{
    return istype(obj, SYMBOL_TYPE);
}

void check_integer(int64_t obj)
{
    if ((int64_t)obj > (int64_t)~TYPE_MASK)
        abort();
    if ((int64_t)obj < (int64_t)TYPE_MASK)
        abort();
}

lisp_object_t integerp(lisp_object_t obj)
{
    uint64_t x = obj & TYPE_MASK;
    return x == 0 || x == TYPE_MASK ? T : NIL;
}

lisp_object_t consp(lisp_object_t obj)
{
    return istype(obj, CONS_TYPE);
}

lisp_object_t vectorp(lisp_object_t obj)
{
    return istype(obj, VECTOR_TYPE);
}

lisp_object_t function_pointer_p(lisp_object_t obj)
{
    return istype(obj, FUNCTION_POINTER_TYPE);
}

lisp_object_t functionp(lisp_object_t obj)
{
    return consp(obj) != NIL && (eq(car(obj), interp->syms.lambda) != NIL || eq(car(obj), interp->syms.built_in_function) != NIL) ? T : NIL;
}

lisp_object_t allocate_lisp_objects(size_t n)
{
    lisp_object_t result = (lisp_object_t)interp->next_free;
    interp->next_free += n;
    return result;
}

lisp_object_t cons(lisp_object_t car, lisp_object_t cdr)
{
    lisp_object_t new_cons = cons_heap_allocate_cons(&interp->cons_heap);
    new_cons |= CONS_TYPE;
    rplaca(new_cons, car);
    rplacd(new_cons, cdr);
    return new_cons;
}

static lisp_object_t *check_vector_bounds_get_storage(lisp_object_t vector, size_t index)
{
    check_vector(vector);
    struct vector *v = VectorPtr(vector);
    if (index >= v->len) {
        printf("Index %zu out of bounds for vector (len=%lu)\n", index, v->len);
        abort();
    }
    lisp_object_t *storage = (lisp_object_t *)v->storage;
    return storage;
}

lisp_object_t svref(lisp_object_t vector, size_t index)
{
    lisp_object_t *storage = check_vector_bounds_get_storage(vector, index);
    return storage[index];
}

lisp_object_t svref_set(lisp_object_t vector, size_t index, lisp_object_t newvalue)
{
    lisp_object_t *storage = check_vector_bounds_get_storage(vector, index);
    storage[index] = newvalue;
    return newvalue;
}

lisp_object_t allocate_vector(size_t size)
{
    /* Allocate header */
    struct vector *v = (struct vector *)allocate_lisp_objects(2);
    v->len = size;
    /* Allocate storage */
    v->storage = allocate_lisp_objects(size);
    lisp_object_t result = (lisp_object_t)v | VECTOR_TYPE;
    for (int i = 0; i < size; i++)
        svref_set(result, i, NIL);
    return result;
}

static void define_built_in_function(char *symbol_name, void (*function_pointer)(void), int arity)
{
    struct symbol *symptr = SymbolPtr(sym(symbol_name));
    lisp_object_t fp = ((uint64_t)function_pointer) | FUNCTION_POINTER_TYPE;
    symptr->function = cons(interp->syms.built_in_function, cons(fp, cons(arity, NIL)));
}

void init_interpreter(size_t heap_size)
{
    interp = (struct lisp_interpreter *)malloc(sizeof(struct lisp_interpreter));
    assert(sizeof(lisp_object_t) == sizeof(void *));
    interp->heap_size_bytes = heap_size * sizeof(lisp_object_t);
    interp->heap = mmap((void *)0x100000000000, interp->heap_size_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (interp->heap == (lisp_object_t *)-1) {
        perror("init_interpreter: mmap failed");
        exit(1);
    }
    bzero(interp->heap, interp->heap_size_bytes);
    interp->next_free = interp->heap;
    cons_heap_init(&interp->cons_heap, heap_size);
    interp->symbol_table = NIL;
    interp->syms.lambda = sym("lambda");
    interp->syms.quote = sym("quote");
    interp->syms.cond = sym("cond");
    interp->syms.defun = sym("defun");
    interp->syms.built_in_function = sym("built-in-function");
    interp->syms.prog = sym("prog");
    interp->syms.progn = sym("progn");
    interp->syms.tagbody = sym("tagbody");
    interp->syms.set = sym("set");
    interp->syms.go = sym("go");
    interp->syms.return_ = sym("return");
    interp->syms.amprest = sym("&rest");
    interp->syms.ampbody = sym("&body");
    interp->syms.ampoptional = sym("&optional");
    interp->syms.condition_case = sym("condition-case");
    interp->syms.defmacro = sym("defmacro");
    interp->syms.quasiquote = sym("quasiquote");
    interp->syms.unquote = sym("unquote");
    interp->syms.unquote_splice = sym("unquote-splice");
    interp->syms.let = sym("let");
    interp->environ = NIL;
#define DEFBUILTIN(S, F, A) define_built_in_function(S, (void (*)())F, A)
    DEFBUILTIN("car", car, 1);
    DEFBUILTIN("cdr", cdr, 1);
    DEFBUILTIN("cons", cons, 2);
    DEFBUILTIN("atom", atom, 1);
    DEFBUILTIN("eq", eq, 2);
    DEFBUILTIN("load", load, 1);
    DEFBUILTIN("read", lisp_read, 0);
    DEFBUILTIN("print", print, 1);
    DEFBUILTIN("princ", princ, 1);
    DEFBUILTIN("eval", eval_toplevel, 1);
    DEFBUILTIN("rplaca", rplaca, 2);
    DEFBUILTIN("rplacd", rplacd, 2);
    DEFBUILTIN("two-arg-plus", plus, 2);
    DEFBUILTIN("two-arg-minus", minus, 2);
    DEFBUILTIN("=", eq, 2);
    DEFBUILTIN("raise", raise, 2);
    DEFBUILTIN("exit", exit, 1);
    DEFBUILTIN("get", getprop, 2);
    DEFBUILTIN("putprop", putprop, 3);
#undef DEFBUILTIN
    interp->prog_return_stack = NULL;
    interpreter_initialized = 1;
    top_of_stack = NULL;
}

void cons_heap_init(struct cons_heap *cons_heap, size_t size)
{
    cons_heap->size = size;
    cons_heap->allocation_count = 0;
    cons_heap->actual_heap = mmap((void *)0x200000000000, sizeof(struct cons) * size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (cons_heap->actual_heap == (struct cons *)-1) {
        perror("cons_heap_init: mmap failed");
        exit(1);
    }
    cons_heap->free_list_head = cons_heap->actual_heap;
    struct cons *p;
    for (int i = 0; i < size; i++) {
        p = cons_heap->actual_heap + i;
        p->mark_bit = 0;
        p->is_allocated = 0;
        p->car = NIL;
        p->cdr = NIL;
    }
    for (int i = 0; i < size - 1; i++) {
        p = cons_heap->actual_heap + i;
        p->cdr = (lisp_object_t)(p + 1);
    }
}

void cons_heap_free(struct cons_heap *cons_heap)
{
    int rc = munmap(cons_heap->actual_heap, cons_heap->size * sizeof(struct cons));
    if (rc != 0) {
        perror("cons_heap_free: munmap failed");
        exit(1);
    }
}

void *get_rbp(int offset)
{
    uint64_t dummy = 0;
    uint64_t *rbp = (&dummy) + 2;
    for (int i = 0; i < offset; i++)
        rbp = *((uint64_t **)rbp);
    return rbp;
}

static void mark_object(lisp_object_t obj)
{
    if (obj == NIL || obj == T) {
        return;
    } else if (consp(obj) == T) {
        struct cons *cons_ptr = ConsPtr(obj);
        if (cons_ptr->mark_bit)
            return;
        cons_ptr->mark_bit = 1;
        mark_object(cons_ptr->car);
        mark_object(cons_ptr->cdr);
    } else if (symbolp(obj) == T) {
        struct symbol *sym = SymbolPtr(obj);
        mark_object(sym->function);
        mark_object(sym->name);
        mark_object(sym->value);
    }
}

void mark_stack(struct cons_heap *cons_heap)
{
    void *rbp = get_rbp(1);
    assert(top_of_stack);
    for (lisp_object_t *s = top_of_stack; s > (lisp_object_t *)rbp; s--) {
        if (consp(*s)) {
            struct cons *p = ConsPtr(*s);
            struct cons *cons_heap_limit = cons_heap->actual_heap + cons_heap->size;
            if (p >= cons_heap->actual_heap && p < cons_heap_limit && p->is_allocated)
                mark_object(*s);
        }
    }
}

void mark(struct cons_heap *cons_heap)
{
    mark_object(interp->environ);
    mark_object(interp->symbol_table);
    mark_stack(cons_heap);
}

void sweep(struct cons_heap *cons_heap)
{
    for (int i = 0; i < cons_heap->size; i++) {
        struct cons *p = cons_heap->actual_heap + i;
        assert(!(p->mark_bit && !p->is_allocated));
        if (p->is_allocated && !p->mark_bit) {
            p->cdr = (lisp_object_t)cons_heap->free_list_head;
            p->is_allocated = 0;
            cons_heap->free_list_head = p;
            cons_heap->allocation_count--;
        } else {
            p->mark_bit = 0;
        }
    }
}

static void gc(struct cons_heap *cons_heap)
{
    size_t before = cons_heap->allocation_count;
    mark(cons_heap);
    sweep(cons_heap);
    size_t nfreed = before - cons_heap->allocation_count;
    printf("; Garbage collection: %lu conses freed\n", nfreed);
}

lisp_object_t cons_heap_allocate_cons(struct cons_heap *cons_heap)
{
    if ((lisp_object_t)cons_heap->free_list_head == NIL)
        gc(cons_heap);
    if ((lisp_object_t)cons_heap->free_list_head == NIL)
        abort();
    struct cons *the_cons = cons_heap->free_list_head;
    cons_heap->free_list_head = (struct cons *)the_cons->cdr;
    the_cons->car = NIL;
    the_cons->cdr = NIL;
    the_cons->is_allocated = 1;
    assert(!the_cons->mark_bit);
    cons_heap->allocation_count++;
    return (lisp_object_t)the_cons;
}

void free_interpreter()
{
    if (interpreter_initialized) {
        cons_heap_free(&interp->cons_heap);
        int rc = munmap(interp->heap, interp->heap_size_bytes);
        if (rc != 0) {
            perror("free_interpreter: munmap failed");
            exit(1);
        }
        free(interp);
        interpreter_initialized = 0;
    }
}

/* len here includes the terminating null byte of str */
lisp_object_t allocate_string(size_t len, char *str)
{
    size_t size = 2 + (len - 1) / 8;
    lisp_object_t obj = allocate_lisp_objects(size);
    size_t *header_address = (size_t *)obj;
    *header_address = len;
    char *straddr = (char *)(header_address + 1);
    strncpy(straddr, str, len);
    return obj | STRING_TYPE;
}

void get_string_parts(lisp_object_t string, size_t *lenptr, char **strptr)
{
    check_string(string);
    size_t *name_length_ptr = StringPtr(string);
    *lenptr = *name_length_ptr - 1;
    *strptr = (char *)(name_length_ptr + 1);
}

lisp_object_t string_equalp(lisp_object_t s1, lisp_object_t s2)
{
    check_string(s1);
    check_string(s2);
    if (eq(s1, s2) != NIL) {
        return T;
    } else {
        /* Compare lengths */
        size_t *l1p = (size_t *)(s1 & PTR_MASK);
        size_t *l2p = (size_t *)(s2 & PTR_MASK);
        if (*l1p != *l2p)
            return NIL;
        char *str1 = (char *)(l1p + 1);
        char *str2 = (char *)(l2p + 1);
        return strncmp(str1, str2, *l1p) == 0 ? T : NIL;
    }
}

lisp_object_t symbol_name(lisp_object_t sym)
{
    check_symbol(sym);
    return SymbolPtr(sym)->name;
}

lisp_object_t find_symbol(lisp_object_t list_of_symbols, lisp_object_t name)
{
    check_string(name);
    if (list_of_symbols == NIL)
        return NIL;
    check_cons(list_of_symbols);
    lisp_object_t first = car(list_of_symbols);
    if (string_equalp(symbol_name(first), name) == T)
        return first;
    else
        return find_symbol(cdr(list_of_symbols), name);
}

lisp_object_t allocate_symbol(lisp_object_t name)
{
    lisp_object_t preexisting_symbol = find_symbol(interp->symbol_table, name);
    if (preexisting_symbol != NIL) {
        return preexisting_symbol;
    } else {
        check_string(name);
        lisp_object_t obj = allocate_lisp_objects(4);
        struct symbol *s = (struct symbol *)obj;
        s->name = name;
        s->value = NIL;
        s->function = NIL;
        s->plist = NIL;
        lisp_object_t symbol = obj | SYMBOL_TYPE;
        interp->symbol_table = cons(symbol, interp->symbol_table);
        return symbol;
    }
}

lisp_object_t getprop(lisp_object_t sym, lisp_object_t ind)
{
    check_symbol(sym);
    struct symbol *symptr = SymbolPtr(sym);
    for (lisp_object_t o = symptr->plist; o != NIL; o = cdr(o)) {
        if (eq(car(car(o)), ind) != NIL)
            return cdr(car(o));
    }
    return NIL;
}

lisp_object_t putprop(lisp_object_t sym, lisp_object_t ind, lisp_object_t value)
{
    check_symbol(sym);
    struct symbol *symptr = SymbolPtr(sym);
    for (lisp_object_t o = symptr->plist; o != NIL; o = cdr(o)) {
        if (eq(car(o), ind) != NIL) {
            rplacd(o, value);
            return value;
        }
    }
    symptr->plist = cons(cons(ind, value), symptr->plist);
    return value;
}

lisp_object_t parse_symbol(char *str)
{
    if (strcmp(str, "nil") == 0)
        return NIL;
    else if (strcmp(str, "t") == 0)
        return T;
    else
        return allocate_symbol(allocate_string(strlen(str) + 1 /* include terminating null */, str));
}

static char tspeek(struct text_stream *ts)
{
    if (text_stream_eof(ts))
        return raise(sym("end-of-file"), NIL);
    else
        return text_stream_peek(ts);
}

void skip_whitespace(struct text_stream *ts)
{
    while (!text_stream_eof(ts) && strchr("\r\n\t ", tspeek(ts)) != NULL)
        text_stream_advance(ts);
    if (text_stream_eof(ts))
        return;
    if (tspeek(ts) == ';') {
        while (!text_stream_eof(ts) && tspeek(ts) != '\n')
            text_stream_advance(ts);
        skip_whitespace(ts);
    }
}

char *read_token(struct text_stream *ts)
{
    struct string_buffer sb;
    string_buffer_init(&sb);
    char *str = alloca(2);
    while (!text_stream_eof(ts) && strchr("\r\n\t )(", tspeek(ts)) == NULL) {
        char ch = tspeek(ts);
        str[0] = ch;
        str[1] = '\0';
        string_buffer_append(&sb, str);
        text_stream_advance(ts);
    }
    char *result = string_buffer_to_string(&sb);
    string_buffer_free_links(&sb);
    return result;
}

lisp_object_t parse_cons(struct text_stream *ts)
{
    skip_whitespace(ts);
    lisp_object_t new_cons = cons(parse1(ts), NIL);
    skip_whitespace(ts);
    if (tspeek(ts) == '.') {
        text_stream_advance(ts);
        skip_whitespace(ts);
        rplacd(new_cons, parse1(ts));
    }
    if (tspeek(ts) == ')') {
        text_stream_advance(ts);
        return new_cons;
    } else {
        rplacd(new_cons, parse_cons(ts));
    }
    return new_cons;
}

/* Returns a C int, not a Lisp integer */
int length_c(lisp_object_t seq)
{
    int result = 0;
    if (seq == NIL)
        return result;
    if (vectorp(seq) != NIL) {
        struct vector *v = VectorPtr(seq);
        return v->len;
    } else if (consp(seq) != NIL) {
        check_cons(seq);
        lisp_object_t obj;
        for (obj = seq; obj != NIL; obj = cdr(obj))
            result++;
    } else {
        abort();
    }
    return result;
}

lisp_object_t parse_vector(struct text_stream *ts)
{
    assert(tspeek(ts) == '(');
    text_stream_advance(ts);
    lisp_object_t list = parse_cons(ts);
    int len = length_c(list);
    lisp_object_t vector = allocate_vector(len);
    /* Copy the list into a vector */
    int i;
    lisp_object_t c;
    for (i = 0, c = list; i < len; i++, c = cdr(c))
        svref_set(vector, i, car(c));
    return vector;
}

lisp_object_t parse_dispatch(struct text_stream *ts)
{
    assert(tspeek(ts) == '#');
    text_stream_advance(ts);
    if (!tspeek(ts))
        abort();
    assert(tspeek(ts) == '(');
    return parse_vector(ts);
}

lisp_object_t parse1(struct text_stream *ts)
{
    skip_whitespace(ts);
    if (!tspeek(ts)) {
        abort();
    } else if (tspeek(ts) == '\'') {
        text_stream_advance(ts);
        return cons(interp->syms.quote, cons(parse1(ts), NIL));
    } else if (tspeek(ts) == '`') {
        text_stream_advance(ts);
        return cons(interp->syms.quasiquote, cons(parse1(ts), NIL));
    } else if (tspeek(ts) == ',') {
        text_stream_advance(ts);
        if (tspeek(ts) == '@') {
            text_stream_advance(ts);
            return cons(interp->syms.unquote_splice, cons(parse1(ts), NIL));
        } else {
            return cons(interp->syms.unquote, cons(parse1(ts), NIL));
        }
    } else if (tspeek(ts) == '(') {
        text_stream_advance(ts);
        skip_whitespace(ts);
        if (tspeek(ts) == ')') {
            /* Special case for () */
            text_stream_advance(ts);
            return NIL;
        } else {
            return parse_cons(ts);
        }
    } else if (tspeek(ts) == ')') {
        printf("Unexpected close paren\n");
    } else if (tspeek(ts) == '#') {
        return parse_dispatch(ts);
    } else if (tspeek(ts) == '"') {
        return parse_string(ts);
    } else {
        char *token = read_token(ts);
        char *zeroxprefix = strstr(token, "0x");
        int base = zeroxprefix == token ? 16 : 10;
        char *endptr;
        uint64_t val = strtoll(token, &endptr, base);
        if (*endptr == '\0') {
            return base == 16 ? val | FUNCTION_POINTER_TYPE : val;
        } else {
            lisp_object_t sym = parse_symbol(token);
            free(token);
            return sym;
        }
    }
    return 0;
}

lisp_object_t parse1_handle_eof(struct text_stream *ts, int *eof)
{
    skip_whitespace(ts);
    if (text_stream_eof(ts)) {
        *eof = 1;
        return NIL;
    }
    return parse1(ts);
}

void parse(struct text_stream *ts, void (*callback)(void *, lisp_object_t), void *callback_data)
{
    while (!text_stream_eof(ts)) {
        int eof = 0;
        lisp_object_t result = parse1_handle_eof(ts, &eof);
        if (eof)
            return;
        else
            callback(callback_data, result);
    }
}

/* Convenience function */
lisp_object_t sym(char *string)
{
    return parse_symbol(string);
}

/* Printing */

char *print_object(lisp_object_t obj)
{
    struct string_buffer sb;
    string_buffer_init(&sb);
    print_object_to_buffer(obj, &sb);
    char *result = string_buffer_to_string(&sb);
    string_buffer_free_links(&sb);
    return result;
}

void print_cons_to_buffer(lisp_object_t obj, struct string_buffer *sb)
{
    print_object_to_buffer(car(obj), sb);
    if (cdr(obj) != NIL) {
        if (consp(cdr(obj)) != NIL) {
            string_buffer_append(sb, " ");
            print_cons_to_buffer(cdr(obj), sb);
        } else {
            string_buffer_append(sb, " . ");
            print_object_to_buffer(cdr(obj), sb);
        }
    }
}

void print_object_to_buffer(lisp_object_t obj, struct string_buffer *sb)
{
    if (integerp(obj) != NIL) {
        int64_t value = obj;
        int length = snprintf(NULL, 0, "%ld", value);
        char *str = alloca(length + 1);
        snprintf(str, length + 1, "%ld", value);
        string_buffer_append(sb, str);
    } else if (obj == NIL) {
        string_buffer_append(sb, "nil");
    } else if (obj == T) {
        string_buffer_append(sb, "t");
    } else if (consp(obj) != NIL) {
        string_buffer_append(sb, "(");
        print_cons_to_buffer(obj, sb);
        string_buffer_append(sb, ")");
    } else if (symbolp(obj) != NIL) {
        check_symbol(obj);
        struct symbol *sym = SymbolPtr(obj);
        size_t len;
        char *strptr;
        get_string_parts(sym->name, &len, &strptr);
        char *tmp = alloca(len + 1);
        strncpy(tmp, strptr, len);
        tmp[len] = 0;
        string_buffer_append(sb, tmp);
    } else if (stringp(obj) != NIL) {
        size_t len = 0;
        char *str = NULL;
        get_string_parts(obj, &len, &str);
        string_buffer_append(sb, str);
    } else if (vectorp(obj) != NIL) {
        int len = length_c(obj);
        string_buffer_append(sb, "#(");
        for (int i = 0; i < len - 1; i++) {
            print_object_to_buffer(svref(obj, i), sb);
            string_buffer_append(sb, " ");
        }
        print_object_to_buffer(svref(obj, len - 1), sb);
        string_buffer_append(sb, ")");
    } else if (function_pointer_p(obj) != NIL) {
        char *buf = alloca(32);
        sprintf(buf, "%p", (FunctionPtr(obj)));
        string_buffer_append(sb, buf);
    }
}

lisp_object_t parse_string(struct text_stream *ts)
{
    assert(tspeek(ts) == '"');
    text_stream_advance(ts);
    int len = 0;
    int escaped = 0;
    struct string_buffer sb;
    string_buffer_init(&sb);
    for (; escaped || tspeek(ts) != '"'; text_stream_advance(ts)) {
        char p = tspeek(ts);
        if (!escaped && p == '\\') {
            escaped = 1;
        } else {
            char c;
            if (escaped) {
                if (p == '\\')
                    c = '\\';
                else if (p == 'n')
                    c = '\n';
                else if (p == 'r')
                    c = '\r';
                else if (p == 't')
                    c = '\t';
                else if (p == '"')
                    c = '"';
                else {
                    printf("Unknown escape character: %c\n", p);
                    abort();
                }
                escaped = 0;
            } else
                c = p;
            char *tmp = (char *)alloca(2);
            tmp[0] = c;
            tmp[1] = '\0';
            string_buffer_append(&sb, tmp);
            len++;
        }
    }
    assert(tspeek(ts) == '"');
    text_stream_advance(ts); /* Move past closing " */
    char *str = string_buffer_to_string(&sb);
    lisp_object_t result = allocate_string(len + 1, str);
    string_buffer_free_links(&sb);
    free(str);
    return result;
}

/* Evaluation */

lisp_object_t caar(lisp_object_t obj)
{
    return car(car(obj));
}

lisp_object_t cadr(lisp_object_t obj)
{
    return car(cdr(obj));
}

lisp_object_t cdar(lisp_object_t obj)
{
    return cdr(car(obj));
}

lisp_object_t cddr(lisp_object_t obj)
{
    return cdr(cdr(obj));
}

lisp_object_t caddr(lisp_object_t obj)
{
    return car(cdr(cdr(obj)));
}

lisp_object_t cadar(lisp_object_t obj)
{
    return car(cdr(car(obj)));
}

lisp_object_t atom(lisp_object_t obj)
{
    return consp(obj) != NIL ? NIL : T;
}

lisp_object_t sub2(lisp_object_t a, lisp_object_t z)
{
    if (a == NIL)
        return z;
    else if (eq(caar(a), z) != NIL)
        return cdar(a);
    else
        return sub2(cdr(a), z);
}

lisp_object_t sublis(lisp_object_t a, lisp_object_t y)
{
    if (atom(y) != NIL)
        return sub2(a, y);
    else
        return cons(sublis(a, car(y)), sublis(a, cdr(y)));
}

lisp_object_t null(lisp_object_t obj)
{
    return (obj == NIL) ? T : NIL;
}

lisp_object_t append(lisp_object_t x, lisp_object_t y)
{
    if (null(x) != NIL)
        return y;
    else
        return cons(car(x), append(cdr(x), y));
}

lisp_object_t member(lisp_object_t x, lisp_object_t y)
{
    if (null(y) != NIL)
        return NIL;
    else if (eq(x, car(y)) != NIL)
        return T;
    else
        return member(x, cdr(y));
}

lisp_object_t assoc(lisp_object_t x, lisp_object_t a)
{
    if (null(a) != NIL) /* McCarthy's ASSOC does not have this! */
        return NIL;
    if (eq(caar(a), x) != NIL)
        return car(a);
    else
        return assoc(x, cdr(a));
}

lisp_object_t pairlis2(lisp_object_t x, lisp_object_t y, lisp_object_t a)
{
    if (x == NIL)
        return a;
    else if (eq(car(x), interp->syms.amprest) != NIL || eq(car(x), interp->syms.ampbody) != NIL)
        return cons(cons(cadr(x), y), a);
    else if (eq(car(x), interp->syms.ampoptional) != NIL)
        return cons(cons(cadr(x), car(y)), NIL);
    else
        return cons(cons(car(x), car(y)), pairlis2(cdr(x), cdr(y), a));
}

lisp_object_t pairlis(lisp_object_t x, lisp_object_t y, lisp_object_t a)
{

    if (null(x) != NIL)
        return a;
    else
        return cons(cons(car(x), car(y)), pairlis(cdr(x), cdr(y), a));
}

static void push_return_context(lisp_object_t type)
{
    struct return_context *ctxt = malloc(sizeof(struct return_context));
    ctxt->type = type;
    ctxt->next = interp->prog_return_stack;
    ctxt->return_value = NIL;
    ctxt->tagbody_forms = NULL;
    interp->prog_return_stack = ctxt;
}

static lisp_object_t pop_return_context()
{
    struct return_context *ctxt = interp->prog_return_stack;
    lisp_object_t retval = ctxt->return_value;
    interp->prog_return_stack = ctxt->next;
    if (ctxt->tagbody_forms)
        free(ctxt->tagbody_forms);
    free(ctxt);
    return retval;
}

lisp_object_t raise(lisp_object_t sym, lisp_object_t value)
{
    while (interp->prog_return_stack && interp->prog_return_stack->type != sym)
        pop_return_context();
    if (!interp->prog_return_stack) {
        char *message = print_object(cons(sym, value));
        abort();
    }
    interp->prog_return_stack->return_value = value;
    longjmp(interp->prog_return_stack->buf, 1);
    return NIL; /* we never actually return */
}

lisp_object_t apply(lisp_object_t fn, lisp_object_t x, lisp_object_t a)
{
    if (atom(fn) != NIL) {
        if (fn == NIL)
            abort();
        struct symbol *sym = SymbolPtr(fn);
        if (sym->function != NIL)
            /* Function cell of symbol is bound */
            return apply(sym->function, x, a);
        else
            return apply(eval(fn, a), x, a);
    } else if (eq(car(fn), interp->syms.lambda) != NIL) {
        push_return_context(interp->syms.return_);
        if (setjmp(interp->prog_return_stack->buf)) {
            return pop_return_context();
        } else {
            lisp_object_t retval;
            lisp_object_t env = pairlis2(cadr(fn), x, a);
            for (lisp_object_t expr = cddr(fn); expr != NIL; expr = cdr(expr))
                retval = eval(car(expr), env);
            /* If we get here, we never longjmped */
            pop_return_context();
            return retval;
        }
    } else if (eq(car(fn), interp->syms.built_in_function) != NIL) {
        check_function_pointer(cadr(fn));
        void (*fp)() = FunctionPtr(cadr(fn));
        lisp_object_t arity = caddr(fn);
        switch (arity) {
        case 0:
            return ((lisp_object_t(*)())fp)();
        case 1:
            return ((lisp_object_t(*)(lisp_object_t))fp)(car(x));
        case 2:
            return ((lisp_object_t(*)(lisp_object_t, lisp_object_t))fp)(car(x), cadr(x));
        case 3:
            return ((lisp_object_t(*)(lisp_object_t, lisp_object_t, lisp_object_t))fp)(car(x), cadr(x), caddr(x));
        default:
            abort();
        }
    } else {
        char *str = print_object(fn);
        printf("Bad function: %s\n", str);
        free(str);
        exit(1);
    }
}

lisp_object_t evcon(lisp_object_t c, lisp_object_t a)
{
    if (eval(caar(c), a) != NIL)
        return eval(cadar(c), a);
    else
        return evcon(cdr(c), a);
}

lisp_object_t evlis(lisp_object_t m, lisp_object_t a)
{
    if (null(m) != NIL)
        return NIL;
    else
        return cons(eval(car(m), a), evlis(cdr(m), a));
}

lisp_object_t evallet(lisp_object_t e, lisp_object_t a)
{
    lisp_object_t extended_env = a;
    for (lisp_object_t varlist = car(e); varlist != NIL; varlist = cdr(varlist)) {
        lisp_object_t entry = car(varlist);
        if (consp(entry) != NIL)
            extended_env = cons(cons(car(entry), eval(cadr(entry), a)), extended_env);
        else
            extended_env = cons(cons(entry, NIL), extended_env);
    }
    return eval(cons(interp->syms.progn, cdr(e)), extended_env);
}

lisp_object_t evaldefun(lisp_object_t e, lisp_object_t a)
{
    lisp_object_t fname = car(e);
    lisp_object_t arglist = cadr(e);
    lisp_object_t body = caddr(e);
    lisp_object_t fn = cons(interp->syms.lambda, cons(arglist, cons(body, NIL)));
    struct symbol *sym = SymbolPtr(fname);
    sym->function = fn;
    return fname;
}

lisp_object_t evaldefmacro(lisp_object_t e, lisp_object_t a)
{
    lisp_object_t fname = evaldefun(e, a);
    putprop(fname, sym("macro"), T);
    return fname;
}

lisp_object_t evalset(lisp_object_t e, lisp_object_t a)
{
    lisp_object_t sym = eval(cadr(e), a);
    check_symbol(sym);
    lisp_object_t new_value = eval(caddr(e), a);
    lisp_object_t x = assoc(sym, a);
    if (x == NIL)
        abort();
    rplacd(x, new_value);
    return new_value;
}

static lisp_object_t extend_env_for_prog(lisp_object_t varlist, lisp_object_t a)
{
    if (varlist == NIL)
        return a;
    else
        return extend_env_for_prog(cdr(varlist), cons(cons(car(varlist), NIL), a));
}

lisp_object_t evalprog(lisp_object_t e, lisp_object_t a)
{
    lisp_object_t extended_env = extend_env_for_prog(car(e), a);
    int n = 0;
    for (lisp_object_t x = cdr(e); x != NIL; x = cdr(x)) {
        if (symbolp(car(x)) == NIL)
            n++;
    }
    lisp_object_t *table = alloca(n * sizeof(lisp_object_t));
    int i = 0;
    lisp_object_t alist = NIL;
    for (lisp_object_t x = cdr(e); x != NIL; x = cdr(x)) {
        if (symbolp(car(x)) == NIL) {
            table[i] = car(x);
            i++;
        } else {
            alist = cons(cons(car(x), i), alist);
        }
    }
    push_return_context(interp->syms.return_);
    if (setjmp(interp->prog_return_stack->buf)) {
        return pop_return_context();
    } else {
        for (i = 0; i < n;) {
            lisp_object_t code = table[i];
            if (consp(code) != NIL && car(code) == interp->syms.go) {
                lisp_object_t target = assoc(cadr(code), alist);
                if (target != NIL)
                    i = cdr(target);
                else
                    abort();
            } else {
                eval(code, extended_env);
                i++;
            }
        }
        /* If we get here, we never longjmped */
        pop_return_context();
        return NIL;
    }
}

lisp_object_t evalprogn(lisp_object_t e, lisp_object_t a)
{
    lisp_object_t return_value = NIL;
    for (lisp_object_t o = e; o != NIL; o = cdr(o))
        return_value = eval(car(o), a);
    return return_value;
}

lisp_object_t evaltagbody(lisp_object_t e, lisp_object_t a)
{
    /* count forms that are not tags */
    int n = 0;
    for (lisp_object_t x = e; x != NIL; x = cdr(x)) {
        if (symbolp(car(x)) == NIL)
            n++;
    }
    push_return_context(interp->syms.tagbody);
    /* forms in an array */
    lisp_object_t *table = malloc(n * sizeof(lisp_object_t));
    int i = 0;
    /* alist tag -> array index */
    lisp_object_t alist = NIL;
    for (lisp_object_t x = e; x != NIL; x = cdr(x)) {
        if (symbolp(car(x)) == NIL)
            /* not a symbol - add form to table */
            table[i++] = car(x);
        else
            /* add symbol -> table index mapping to alist */
            alist = cons(cons(car(x), i), alist);
    }
    interp->prog_return_stack->tagbody_forms = table;
    interp->prog_return_stack->return_value = alist;
    for (i = 0; i < n; i++) {
        int v = setjmp(interp->prog_return_stack->buf);
        if (v != 0)
            i = v - 1;
        eval(table[i], a);
    }
    pop_return_context();
    return NIL;
}

lisp_object_t evalgo(lisp_object_t tag)
{
    while (interp->prog_return_stack && eq(interp->prog_return_stack->type, interp->syms.return_) == NIL && eq(interp->prog_return_stack->type, interp->syms.tagbody) == NIL)
        pop_return_context();
    struct return_context *ctxt = interp->prog_return_stack;
    if (ctxt && eq(ctxt->type, interp->syms.tagbody) != NIL) {
        longjmp(ctxt->buf, cdr(assoc(tag, ctxt->return_value)) + 1);
    } else {
        /* No enclosing tagbody */
        if (ctxt)
            assert(eq(ctxt->type, interp->syms.return_) != NIL);
        raise(sym("error"), NIL);
    }
    return NIL; /* never actually returned */
}

lisp_object_t eval_condition_case(lisp_object_t e, lisp_object_t a)
{
    lisp_object_t var = car(e);
    lisp_object_t code = cadr(e);
    lisp_object_t handlers = cddr(e);
    for (lisp_object_t handler = handlers; handler != NIL; handler = cdr(handler)) {
        lisp_object_t symbol = caar(handler);
        push_return_context(symbol);
        if (setjmp(interp->prog_return_stack->buf)) {
            symbol = interp->prog_return_stack->type;
            lisp_object_t entry = cons(var, cons(symbol, cons(pop_return_context(), NIL)));
            lisp_object_t env = cons(entry, a);
            return eval(cadr(assoc(symbol, handlers)), env);
        }
    }
    return eval(code, a);
}

lisp_object_t eval_quasiquote(lisp_object_t e, lisp_object_t a)
{
    if (e == NIL) {
        return NIL;
    } else if (atom(e) != NIL) {
        return e;
    } else if (consp(e) != NIL) {
        if (eq(car(e), interp->syms.unquote) != NIL) {
            return eval(cadr(e), a);
        } else if (eq(car(e), interp->syms.unquote_splice) != NIL) {
            return eval(cadr(e), a);
        } else if (consp(car(e)) != NIL && eq(car(car(e)), interp->syms.unquote_splice) != NIL) {
            return eval(cadr(car(e)), a);
        } else {
            return cons(eval_quasiquote(car(e), a), eval_quasiquote(cdr(e), a));
        }
    }
    abort();
    return NIL;
}

static lisp_object_t quote_list(lisp_object_t list)
{
    if (list == NIL)
        return NIL;
    else
        return cons(cons(interp->syms.quote, cons(car(list), NIL)), quote_list(cdr(list)));
}

/* This returns a pair as we don't have multiple value return */
/* First element is the macroexpansion, second indicates whether expansion happened */
lisp_object_t macroexpand1(lisp_object_t e, lisp_object_t a)
{
    if (consp(e) != NIL && symbolp(car(e)) != NIL && getprop(car(e), sym("macro")) != NIL) {
        struct symbol *symptr = SymbolPtr(car(e));
        return cons(eval(cons(symptr->function, quote_list(cdr(e))), a), T);
    } else {
        return cons(e, NIL);
    }
}

lisp_object_t macroexpand(lisp_object_t e, lisp_object_t a)
{
    lisp_object_t expanded = NIL;
    do {
        lisp_object_t return_values = macroexpand1(e, a);
        e = car(return_values);
        expanded = cdr(return_values);
    } while (expanded != NIL);
    return e;
}

lisp_object_t macroexpand_all(lisp_object_t e);

static lisp_object_t macroexpand_all_list(lisp_object_t list)
{
    if (list == NIL)
        return NIL;
    else
        return cons(macroexpand_all(car(list)), macroexpand_all_list(cdr(list)));
}

static lisp_object_t macroexpand_all_cond_clauses(lisp_object_t clauses)
{
    if (clauses == NIL) {
        return NIL;
    } else {
        lisp_object_t first_clause = car(clauses);
        lisp_object_t a = car(first_clause);
        lisp_object_t b = cadr(first_clause);
        return cons(cons(macroexpand_all(a), cons(macroexpand_all(b), NIL)), macroexpand_all_cond_clauses(cdr(clauses)));
    }
}

static lisp_object_t macroexpand_all_tagbody(lisp_object_t tagbody)
{
    if (tagbody == NIL) {
        return NIL;
    } else if (consp(car(tagbody)) != NIL) { /* not a tag */
        return cons(macroexpand_all(car(tagbody)), macroexpand_all_tagbody(cdr(tagbody)));
    } else {
        return cons(car(tagbody), macroexpand_all_tagbody(cdr(tagbody)));
    }
    return NIL;
}

static lisp_object_t macroexpand_all_let(lisp_object_t vars)
{
    if (vars == NIL) {
        return NIL;
    } else {
        lisp_object_t clause = car(vars);
        if (consp(clause) != NIL) {
            lisp_object_t var = car(clause);
            lisp_object_t val = cadr(clause);
            return cons(cons(var, cons(macroexpand_all(val), NIL)), macroexpand_all_let(cdr(vars)));
        } else {
            return cons(clause, macroexpand_all_let(cdr(vars)));
        }
    }
}

lisp_object_t macroexpand_all(lisp_object_t e)
{
    if (consp(e) == NIL) {
        return e;
    } else if (symbolp(car(e)) != NIL) {
        lisp_object_t sym = car(e);
        if (sym == interp->syms.cond) {
            return cons(sym, macroexpand_all_cond_clauses(cdr(e)));
        } else if (sym == interp->syms.lambda) {
            lisp_object_t arglist = cadr(e);
            lisp_object_t body = cddr(e);
            return cons(sym, cons(arglist, macroexpand_all_list(body)));
        } else if (sym == interp->syms.tagbody) {
            return cons(sym, macroexpand_all_tagbody(cdr(e)));
        } else if (sym == interp->syms.prog) {
            lisp_object_t arglist = cadr(e);
            lisp_object_t body = cddr(e);
            return cons(sym, cons(arglist, macroexpand_all_tagbody(body)));
        } else if (sym == interp->syms.progn) {
            return cons(sym, macroexpand_all_list(cdr(e)));
        } else if (sym == interp->syms.condition_case) {
            lisp_object_t exc = cadr(e);
            lisp_object_t body = caddr(e);
            lisp_object_t clauses = cdr(cddr(e));
            return cons(sym, cons(exc, cons(macroexpand_all(body), macroexpand_all_let(clauses))));
        } else if (sym == interp->syms.let) {
            lisp_object_t body = cddr(e);
            return cons(sym, cons(macroexpand_all_let(cadr(e)), macroexpand_all_list(body)));
        } else if (sym == interp->syms.defun || sym == interp->syms.defmacro) {
            lisp_object_t name = cadr(e);
            lisp_object_t arglist = caddr(e);
            lisp_object_t body = cdr(cddr(e));
            return cons(sym, cons(name, cons(arglist, macroexpand_all_list(body))));
        } else {
            // This covers function calls, but also special forms that look like them,
            // e.g. `go`, `set`.
            return macroexpand(cons(car(e), macroexpand_all_list(cdr(e))), NIL);
        }
    } else {
        return macroexpand_all_list(e);
    }
}

lisp_object_t eval(lisp_object_t e, lisp_object_t a)
{
    if (e == NIL || e == T || integerp(e) != NIL || vectorp(e) != NIL || stringp(e) != NIL || functionp(e) != NIL)
        return e;
    if (atom(e) != NIL) {
        lisp_object_t x = assoc(e, a);
        if (x == NIL)
            raise(sym("unbound-variable"), e);
        return cdr(x);
    } else if (atom(car(e) != NIL)) {
        if (eq(car(e), interp->syms.quote) != NIL) {
            return car(cdr(e));
        } else if (eq(car(e), interp->syms.quasiquote) != NIL) {
            return eval_quasiquote(cadr(e), a);
        } else if (eq(car(e), interp->syms.unquote) != NIL) {
            abort();
        } else if (eq(car(e), interp->syms.cond) != NIL) {
            return evcon(cdr(e), a);
        } else if (eq(car(e), interp->syms.let) != NIL) {
            return evallet(cdr(e), a);
        } else if (eq(car(e), interp->syms.defun) != NIL) {
            return evaldefun(cdr(e), a);
        } else if (eq(car(e), interp->syms.defmacro) != NIL) {
            return evaldefmacro(cdr(e), a);
        } else if (eq(car(e), interp->syms.set) != NIL) {
            return evalset(e, a);
        } else if (eq(car(e), interp->syms.prog) != NIL) {
            return evalprog(cdr(e), a);
        } else if (eq(car(e), interp->syms.progn) != NIL) {
            return evalprogn(cdr(e), a);
        } else if (eq(car(e), interp->syms.tagbody) != NIL) {
            return evaltagbody(cdr(e), a);
        } else if (eq(car(e), interp->syms.go) != NIL) {
            return evalgo(cadr(e));
        } else if (eq(car(e), interp->syms.return_) != NIL) {
            return raise(interp->syms.return_, eval(cadr(e), a));
        } else if (eq(car(e), interp->syms.condition_case) != NIL) {
            return eval_condition_case(cdr(e), a);
        } else {
            return apply(car(e), evlis(cdr(e), a), a);
        }
    } else {
        return apply(car(e), evlis(cdr(e), a), a);
    }
}

lisp_object_t evalquote(lisp_object_t fn, lisp_object_t x)
{
    return apply(fn, x, NIL);
}

/* Load */

lisp_object_t eval_toplevel(lisp_object_t e)
{
    return eval(macroexpand_all(e), interp->environ);
}

static void load_eval_callback(void *ignored, lisp_object_t obj)
{
    lisp_object_t result = eval_toplevel(obj);
    char *str = print_object(result);
    printf("%s\n", str);
    free(str);
}

lisp_object_t load(lisp_object_t filename)
{
    check_string(filename);
    size_t len;
    char *str;
    get_string_parts(filename, &len, &str);
    load_str(str);
    return T;
}

void load_str(char *str)
{
    int fd = open(str, O_RDONLY);
    if (fd < 0) {
        perror("open");
        abort();
    }
    struct text_stream ts;
    text_stream_init_fd(&ts, fd);
    parse(&ts, load_eval_callback, NULL);
    text_stream_free(&ts);
    int rc = close(fd);
    if (rc < 0) {
        perror("close");
        abort();
    }
}

lisp_object_t lisp_read()
{
    struct text_stream ts;
    text_stream_init_fd(&ts, 1);
    lisp_object_t result = parse1(&ts);
    text_stream_free(&ts);
    return result;
}

lisp_object_t print(lisp_object_t obj)
{
    char *str = print_object(obj);
    printf("%s\n", str);
    fflush(stdout);
    free(str);
    return obj;
}

lisp_object_t princ(lisp_object_t obj)
{
    char *str = print_object(obj);
    printf("%s", str);
    fflush(stdout);
    free(str);
    return obj;
}

lisp_object_t plus(lisp_object_t x, lisp_object_t y)
{
    check_integer(x);
    check_integer(y);
    lisp_object_t result = x + y;
    check_integer(result);
    return result;
}

lisp_object_t minus(lisp_object_t x, lisp_object_t y)
{
    check_integer(x);
    check_integer(y);
    lisp_object_t result = x - y;
    check_integer(result);
    return result;
}