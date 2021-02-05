#include "string_buffer.h"

#include <alloca.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>
#include <unistd.h>

typedef uint64_t lisp_object_t;

#define NIL 0xfffffffffffffff9
#define T 0xfffffffffffffff1

#define TYPE_MASK 0x7
#define PTR_MASK 0xfffffffffffffff8
#define INTEGER_TYPE 0
#define SYMBOL_TYPE 1
#define CONS_TYPE 2
#define STRING_TYPE 3
#define VECTOR_TYPE 4

static char* type_names[5] = { "integer", "symbol", "cons", "string", "vector" };

#define ConsPtr(obj) ((struct cons*)((obj)&PTR_MASK))
#define SymbolPtr(obj) ((struct symbol*)((obj)&PTR_MASK))
/* StringPtr is different as a string is not a struct */
#define StringPtr(obj) ((size_t*)((obj)&PTR_MASK))
#define VectorPtr(obj) ((struct vector*)((obj)&PTR_MASK))

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

struct lisp_interpreter* interp;

static int interpreter_initialized;

struct cons {
    lisp_object_t car;
    lisp_object_t cdr;
};

struct symbol {
    lisp_object_t name;
    lisp_object_t value;
    lisp_object_t function;
};

struct vector {
    size_t len;
    lisp_object_t storage;
};

char* print_object(lisp_object_t obj);

lisp_object_t istype(lisp_object_t obj, int type);

static void check_type(lisp_object_t obj, int type)
{
    if (istype(obj, type) == NIL) {
        char* obj_string = print_object(obj);
        printf("Not a %s: %s\n", type_names[type], obj_string);
        free(obj_string);
        exit(1);
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
    struct cons* p = ConsPtr(the_cons);
    p->car = the_car;
    return the_cons;
}

lisp_object_t rplacd(lisp_object_t the_cons, lisp_object_t the_cdr)
{
    check_cons(the_cons);
    struct cons* p = ConsPtr(the_cons);
    p->cdr = the_cdr;
    return the_cons;
}

lisp_object_t eq(lisp_object_t o1, lisp_object_t o2)
{
    return o1 == o2 ? T : NIL;
}

lisp_object_t istype(lisp_object_t obj, int type)
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

lisp_object_t integerp(lisp_object_t obj)
{
    return istype(obj, INTEGER_TYPE);
}

lisp_object_t consp(lisp_object_t obj)
{
    return istype(obj, CONS_TYPE);
}

lisp_object_t vectorp(lisp_object_t obj)
{
    return istype(obj, VECTOR_TYPE);
}

lisp_object_t allocate_lisp_objects(size_t n)
{
    lisp_object_t result = (lisp_object_t)interp->next_free;
    interp->next_free += n;
    return result;
}

lisp_object_t cons(lisp_object_t car, lisp_object_t cdr)
{
    lisp_object_t new_cons = allocate_lisp_objects(2);
    new_cons |= CONS_TYPE;
    rplaca(new_cons, car);
    rplacd(new_cons, cdr);
    return new_cons;
}

static lisp_object_t* check_vector_bounds_get_storage(lisp_object_t vector, size_t index)
{
    check_vector(vector);
    struct vector* v = VectorPtr(vector);
    if (index >= v->len) {
        printf("Index %zu out of bounds for vector (len=%lu)\n", index, v->len);
        abort();
    }
    lisp_object_t* storage = (lisp_object_t*)v->storage;
    return storage;
}

lisp_object_t svref(lisp_object_t vector, size_t index)
{
    lisp_object_t* storage = check_vector_bounds_get_storage(vector, index);
    return storage[index];
}

lisp_object_t svref_set(lisp_object_t vector, size_t index, lisp_object_t newvalue)
{
    lisp_object_t* storage = check_vector_bounds_get_storage(vector, index);
    storage[index] = newvalue;
    return newvalue;
}

lisp_object_t allocate_vector(size_t size)
{
    /* Allocate header */
    struct vector* v = (struct vector*)allocate_lisp_objects(2);
    v->len = size;
    /* Allocate storage */
    v->storage = allocate_lisp_objects(size);
    lisp_object_t result = (lisp_object_t)v | VECTOR_TYPE;
    for (int i = 0; i < size; i++)
        svref_set(result, i, NIL);
    return result;
}

lisp_object_t sym(char* string);

static void init_interpreter(size_t heap_size)
{
    interp = (struct lisp_interpreter*)malloc(sizeof(struct lisp_interpreter));
    if (sizeof(lisp_object_t) != sizeof(void*))
        abort();
    interp->heap_size_bytes = heap_size * sizeof(lisp_object_t);
    interp->heap = mmap(NULL, interp->heap_size_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (interp->heap == (lisp_object_t*)-1) {
        perror("mmap failed");
        exit(1);
    }
    bzero(interp->heap, interp->heap_size_bytes);
    interp->next_free = interp->heap;
    interp->symbol_table = NIL;
    interp->syms.car = sym("CAR");
    interp->syms.cdr = sym("CDR");
    interp->syms.cons = sym("CONS");
    interp->syms.atom = sym("ATOM");
    interp->syms.eq = sym("EQ");
    interp->syms.lambda = sym("LAMBDA");
    interp->syms.label = sym("LABEL");
    interp->syms.quote = sym("QUOTE");
    interp->syms.cond = sym("COND");
    interp->syms.defun = sym("DEFUN");
    interp->syms.load = sym("LOAD");
    interp->environ = cons(cons(T, T), cons(cons(NIL, NIL), NIL));
    interpreter_initialized = 1;
}

static void free_interpreter()
{
    if (interpreter_initialized) {
        int rc = munmap(interp->heap, interp->heap_size_bytes);
        if (rc != 0) {
            perror("munmap failed");
            exit(1);
        }
        free(interp);
        interpreter_initialized = 0;
    }
}

/* len here includes the terminating null byte of str */
lisp_object_t allocate_string(size_t len, char* str)
{
    size_t size = 2 + (len - 1) / 8;
    lisp_object_t obj = allocate_lisp_objects(size);
    size_t* header_address = (size_t*)obj;
    *header_address = len;
    char* straddr = (char*)(header_address + 1);
    strncpy(straddr, str, len);
    return obj | STRING_TYPE;
}

void get_string_parts(lisp_object_t string, size_t* lenptr, char** strptr)
{
    check_string(string);
    size_t* name_length_ptr = StringPtr(string);
    *lenptr = *name_length_ptr - 1;
    *strptr = (char*)(name_length_ptr + 1);
}

lisp_object_t string_equalp(lisp_object_t s1, lisp_object_t s2)
{
    check_string(s1);
    check_string(s2);
    if (eq(s1, s2) != NIL) {
        return T;
    } else {
        /* Compare lengths */
        size_t* l1p = (size_t*)(s1 & PTR_MASK);
        size_t* l2p = (size_t*)(s2 & PTR_MASK);
        if (*l1p != *l2p)
            return NIL;
        char* str1 = (char*)(l1p + 1);
        char* str2 = (char*)(l2p + 1);
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
        lisp_object_t obj = allocate_lisp_objects(3);
        struct symbol* s = (struct symbol*)obj;
        s->name = name;
        s->value = NIL;
        s->function = NIL;
        lisp_object_t symbol = obj | SYMBOL_TYPE;
        interp->symbol_table = cons(symbol, interp->symbol_table);
        return symbol;
    }
}

lisp_object_t parse_symbol(char** text)
{
    static char* delimiters = " \n\t\r)\0";
    char* p = *text;
    while (!strchr(delimiters, *p))
        p++;
    size_t len = p - *text;
    char* tmp = alloca(len + 1);
    strncpy(tmp, *text, len);
    tmp[len] = 0;
    *text += len;
    if (strcmp(tmp, "nil") == 0) {
        return NIL;
    } else if (strcmp(tmp, "t") == 0) {
        return T;
    } else {
        lisp_object_t lisp_string = allocate_string(len + 1 /* include terminating null */, tmp);
        return allocate_symbol(lisp_string);
    }
}

int parse_integer(char** text)
{
    char* start = *text;
    size_t len = 0;
    for (; **text == '-' || (**text >= '0' && **text <= '9'); (*text)++, len++)
        ;
    char* tmp = (char*)alloca(len + 1);
    strncpy(tmp, start, len);
    tmp[len] = 0;
    return atoi(tmp);
}

static void skip_whitespace(char** text)
{
    while (**text && strchr("\r\n\t ", **text) != NULL)
        (*text)++;
}

lisp_object_t parse1(char**);

lisp_object_t parse_cons(char** text)
{
    skip_whitespace(text);
    lisp_object_t new_cons = cons(parse1(text), NIL);
    skip_whitespace(text);
    if (**text == '.') {
        (*text)++;
        skip_whitespace(text);
        rplacd(new_cons, parse1(text));
    }
    if (**text == ')') {
        (*text)++;
        return new_cons;
    } else {
        rplacd(new_cons, parse_cons(text));
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
        struct vector* v = VectorPtr(seq);
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

lisp_object_t parse_vector(char** text)
{
    if (**text != '(')
        abort();
    if (*(*text - 1) != '#')
        abort();
    (*text)++;
    lisp_object_t list = parse_cons(text);
    int len = length_c(list);
    lisp_object_t vector = allocate_vector(len);
    /* Copy the list into a vector */
    int i;
    lisp_object_t c;
    for (i = 0, c = list; i < len; i++, c = cdr(c))
        svref_set(vector, i, car(c));
    return vector;
}

lisp_object_t parse_dispatch(char** text)
{
    if (**text != '#')
        abort();
    (*text)++;
    if (!**text)
        abort();
    char c = **text;
    if (c == '(')
        return parse_vector(text);
    abort();
}

lisp_object_t parse_string(char**);

lisp_object_t parse1(char** text)
{
    skip_whitespace(text);
    if (!**text)
        return NIL;
    if (**text == '(') {
        (*text)++;
        return parse_cons(text);
    } else if (**text == ')') {
        printf("Unexpected close paren\n");
    } else if (**text == '-' || (**text >= '0' && **text <= '9')) {
        return parse_integer(text) << 3;
    } else if (**text == '#') {
        return parse_dispatch(text);
    } else if (**text == '"') {
        return parse_string(text);
    } else {
        lisp_object_t sym = parse_symbol(text);
        return sym;
    }
    return 0;
}

void parse(char* text, void (*callback)(void*, lisp_object_t), void* callback_data)
{
    char** cursor = &text;
    while (*text)
        callback(callback_data, parse1(cursor));
}

/* Convenience function */
lisp_object_t sym(char* string)
{
    char* tmp = (char*)malloc(strlen(string) + 1);
    char* tmp_save = tmp;
    strcpy(tmp, string);
    lisp_object_t result = parse_symbol(&tmp);
    free(tmp_save);
    return result;
}

/* Printing */

void print_object_to_buffer(lisp_object_t, struct string_buffer*);

char* print_object(lisp_object_t obj)
{
    struct string_buffer sb;
    string_buffer_init(&sb);
    print_object_to_buffer(obj, &sb);
    char* result = string_buffer_to_string(&sb);
    string_buffer_free_links(&sb);
    return result;
}

void debug_obj(lisp_object_t obj)
{
    char* str = print_object(obj);
    printf("%s", str);
    free(str);
}

void debug_obj2(char* msg, lisp_object_t obj)
{
    printf("%s: ", msg);
    debug_obj(obj);
    printf("\n");
}

void print_cons_to_buffer(lisp_object_t, struct string_buffer*);

void print_cons_to_buffer(lisp_object_t obj, struct string_buffer* sb)
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

void print_object_to_buffer(lisp_object_t obj, struct string_buffer* sb)
{
    if (integerp(obj) != NIL) {
        int value = obj >> 3;
        int length = snprintf(NULL, 0, "%d", value);
        char* str = alloca(length + 1);
        snprintf(str, length + 1, "%d", value);
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
        struct symbol* sym = SymbolPtr(obj);
        size_t len;
        char* strptr;
        get_string_parts(sym->name, &len, &strptr);
        char* tmp = alloca(len + 1);
        strncpy(tmp, strptr, len);
        tmp[len] = 0;
        string_buffer_append(sb, tmp);
    } else if (stringp(obj) != NIL) {
        string_buffer_append(sb, "a_string");
    } else if (vectorp(obj) != NIL) {
        int len = length_c(obj);
        string_buffer_append(sb, "#(");
        for (int i = 0; i < len - 1; i++) {
            print_object_to_buffer(svref(obj, i), sb);
            string_buffer_append(sb, " ");
        }
        print_object_to_buffer(svref(obj, len - 1), sb);
        string_buffer_append(sb, ")");
    }
}

lisp_object_t parse_string(char** text)
{
    if (**text != '"')
        abort();
    (*text)++;
    char* p = *text;
    int len = 0;
    int escaped = 0;
    struct string_buffer sb;
    string_buffer_init(&sb);
    for (; escaped || *p != '"'; p++) {
        if (!escaped && *p == '\\') {
            escaped = 1;
        } else {
            char c;
            if (escaped) {
                if (*p == '\\') {
                    c = '\\';
                } else if (*p == 'n') {
                    c = '\n';
                } else if (*p == 'r') {
                    c = '\r';
                } else if (*p == 't') {
                    c = '\t';
                } else if (*p == '"') {
                    c = '"';
                } else {
                    printf("Unknown escape character: %c\n", *p);
                    abort();
                }
                escaped = 0;
            } else {
                c = *p;
            }
            char* tmp = (char*)alloca(2);
            tmp[0] = c;
            tmp[1] = '\0';
            string_buffer_append(&sb, tmp);
            len++;
        }
    }
    char* str = string_buffer_to_string(&sb);
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
    else {
        return cons(car(x), append(cdr(x), y));
    }
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

lisp_object_t pairlis(lisp_object_t x, lisp_object_t y, lisp_object_t a)
{

    if (null(x) != NIL)
        return a;
    else
        return cons(cons(car(x), car(y)), pairlis(cdr(x), cdr(y), a));
}

lisp_object_t eval(lisp_object_t e, lisp_object_t a);

lisp_object_t eval_toplevel(lisp_object_t e)
{
    return eval(e, interp->environ);
}

static void load_eval_callback(void* ignored, lisp_object_t obj)
{
    lisp_object_t result = eval_toplevel(obj);
    char* str = print_object(result);
    printf("; %s\n", str);
    free(str);
}

lisp_object_t load(lisp_object_t filename)
{
    check_string(filename);
    size_t len;
    char* str;
    get_string_parts(filename, &len, &str);
    FILE* f = fopen(str, "r");
    if (f == NULL) {
        perror(str);
        exit(1);
    }
    struct string_buffer sb;
    string_buffer_init(&sb);
    char buf[1024];
    while (fgets(buf, 1024, f) != NULL)
        string_buffer_append(&sb, buf);
    fclose(f);
    char* text = string_buffer_to_string(&sb);
    parse(text, load_eval_callback, NULL);
    free(text);
    string_buffer_free_links(&sb);
    return T;
}

lisp_object_t apply(lisp_object_t fn, lisp_object_t x, lisp_object_t a)
{
    if (atom(fn) != NIL) {
        struct symbol* sym = SymbolPtr(fn);
        if (sym->function != NIL)
            /* Function cell of symbol is bound */
            return apply(sym->function, x, a);
        else if (eq(fn, interp->syms.car) != NIL)
            return caar(x);
        else if (eq(fn, interp->syms.cdr) != NIL)
            return cdar(x);
        else if (eq(fn, interp->syms.cons) != NIL)
            return cons(car(x), cadr(x));
        else if (eq(fn, interp->syms.atom) != NIL)
            return atom(car(x));
        else if (eq(fn, interp->syms.eq) != NIL)
            return eq(car(x), cadr(x));
        else if (eq(fn, interp->syms.load) != NIL)
            return load(car(x));
        else
            return apply(eval(fn, a), x, a);
    } else if (eq(car(fn), interp->syms.lambda) != NIL)
        return eval(caddr(fn), pairlis(cadr(fn), x, a));
    else if (eq(car(fn), interp->syms.label) != NIL)
        return apply(caddr(fn), x, cons(cons(cadr(fn), caddr(fn)), a));
    else {
        char* str = print_object(fn);
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

lisp_object_t evaldefun(lisp_object_t e, lisp_object_t a)
{
    lisp_object_t fname = car(e);
    lisp_object_t arglist = cadr(e);
    lisp_object_t body = caddr(e);
    lisp_object_t fn = cons(interp->syms.lambda, cons(arglist, cons(body, NIL)));
    struct symbol* sym = SymbolPtr(fname);
    sym->function = fn;
    return fname;
}

lisp_object_t eval(lisp_object_t e, lisp_object_t a)
{
    if (e == NIL || e == T || integerp(e) != NIL || vectorp(e) != NIL || stringp(e) != NIL)
        return e;
    if (atom(e) != NIL)
        return cdr(assoc(e, a));
    else if (atom(car(e) != NIL))
        if (eq(car(e), interp->syms.quote) != NIL)
            return cadr(e);
        else if (eq(car(e), interp->syms.cond) != NIL)
            return evcon(cdr(e), a);
        else if (eq(car(e), interp->syms.defun) != NIL)
            return evaldefun(cdr(e), a);
        else
            return apply(car(e), evlis(cdr(e), a), a);
    else
        return apply(car(e), evlis(cdr(e), a), a);
}

lisp_object_t evalquote(lisp_object_t fn, lisp_object_t x)
{
    return apply(fn, x, NIL);
}

/* Testing infrastructure */

static char* test_name; /* Global */

static void check(int boolean, char* tag)
{
    printf("%s - %s", test_name, tag);
    printf("\t");
    if (boolean) {
        printf("ok");
    } else {
        printf("NOT OK");
    }
    printf("\n");
}

/* Unit tests */

static void test_skip_whitespace()
{
    test_name = "skip_whitespace";
    char* test_string = "  hello";
    char* p = test_string;
    skip_whitespace(&p);
    check(p == test_string + 2, "offset");
}

static void test_parse_integer()
{
    test_name = "parse_integer";
    char* test_string = "13";
    int result = parse_integer(&test_string);
    check(result == 13, "value");
}

static void test_parse_negative_integer()
{
    test_name = "parse_negative_integer";
    char* test_string = "-498";
    int result = parse_integer(&test_string);
    check(result == -498, "value");
}

static void test_parse_single_integer_list()
{
    test_name = "parse_single_integer_list";
    char* test_string = "(14)";
    init_interpreter(256);
    lisp_object_t result = parse1(&test_string);
    check(consp(result), "consp");
    lisp_object_t result_car = car(result);
    check(integerp(result_car), "car is int");
    check(result_car == 14 << 3, "car value");
    lisp_object_t result_cdr = cdr(result);
    check(NIL == result_cdr, "cdr is null");
    free_interpreter();
}

static void test_parse_integer_list()
{
    test_name = "parse_integer_list";
    char* test_string = "(23 71)";
    init_interpreter(256);
    lisp_object_t result = parse1(&test_string);
    check(consp(result), "consp");
    lisp_object_t result_car = car(result);
    check(integerp(result_car), "car is int");
    check(result_car == 23 << 3, "car value");
    lisp_object_t result_cdr = cdr(result);
    check(NIL != result_cdr, "cdr is not null");
    check(consp(result_cdr), "cdr is a pair");
    lisp_object_t cadr = car(result_cdr);
    check(integerp(cadr), "cadr is int");
    check(cadr == 71 << 3, "cadr value");
    free_interpreter();
}

static void test_parse_dotted_pair_of_integers()
{
    test_name = "parse_dotted_pair_of_integers";
    char* test_string = "(45 . 123)";
    init_interpreter(256);
    lisp_object_t result = parse1(&test_string);
    check(consp(result), "consp");
    check(integerp(car(result)), "car is int");
    check(integerp(cdr(result)), "cdr is int");
    check(car(result) == 45 << 3, "car value");
    check(cdr(result) == 123 << 3, "cdr value");
    free_interpreter();
}

static void test_string_buffer()
{
    test_name = "string_buffer";
    struct string_buffer sb;
    string_buffer_init(&sb);
    string_buffer_append(&sb, "foo");
    string_buffer_append(&sb, "bar");
    char* string = string_buffer_to_string(&sb);
    check(strcmp("foobar", string) == 0, "string value");
    check(sb.len == 6, "length");
    free(string);
    string_buffer_free_links(&sb);
}

static void test_print_integer()
{
    test_name = "print_integer";
    char* test_string = "93";
    init_interpreter(256);
    lisp_object_t obj = parse1(&test_string);
    char* result = print_object(obj);
    check(strcmp("93", result) == 0, "string value");
    free(result);
    free_interpreter();
}

static void test_print_single_integer_list()
{
    test_name = "print_single_integer_list";
    char* test_string = "(453)";
    init_interpreter(256);
    lisp_object_t obj = parse1(&test_string);
    char* result = print_object(obj);
    check(strcmp("(453)", result) == 0, "string value");
    free(result);
    free_interpreter();
}

static void test_print_integer_list()
{
    test_name = "print_integer_list";
    char* test_string = "(240 -44 902)";
    init_interpreter(256);
    lisp_object_t obj = parse1(&test_string);
    char* result = print_object(obj);
    check(strcmp("(240 -44 902)", result) == 0, "string value");
    free(result);
    free_interpreter();
}

static void test_print_dotted_pair()
{
    test_name = "print_dotted_pair";
    char* test_string = "(65 . 185)";
    init_interpreter(256);
    lisp_object_t obj = parse1(&test_string);
    char* result = print_object(obj);
    check(strcmp("(65 . 185)", result) == 0, "string value");
    free(result);
    free_interpreter();
}

static void test_print_complex_list()
{
    test_name = "print_complex_list";
    char* test_string = "(1 (2 3 4 (5 (6 7 8 (9 . 0)))))";
    init_interpreter(256);
    lisp_object_t obj = parse1(&test_string);
    char* result = print_object(obj);
    check(strcmp("(1 (2 3 4 (5 (6 7 8 (9 . 0)))))", result) == 0, "string value");
    free(result);
    free_interpreter();
}

static void test_nil_is_not_a_cons()
{
    test_name = "nil_is_not_a_cons";
    check(consp(NIL) == NIL, "not a cons");
}

static void test_t_is_not_a_cons()
{
    test_name = "t_is_not_a_cons";
    check(consp(T) == NIL, "not a cons");
}

static void test_nil_is_a_symbol()
{
    test_name = "nil_is_a_symbol";
    check(symbolp(NIL) == T, "symbol");
}

static void test_t_is_a_symbol()
{
    test_name = "t_is_a_symbol";
    check(symbolp(T) == T, "symbol");
}

static void test_read_and_print_nil()
{
    test_name = "read_and_print_nil";
    char* test_string = "nil";
    init_interpreter(256);
    lisp_object_t obj = parse1(&test_string);
    check(obj == NIL, "is nil");
    char* result = print_object(obj);
    check(strcmp("nil", result) == 0, "print nil");
    free(result);
    free_interpreter();
}

static void test_read_and_print_t()
{
    test_name = "read_and_print_t";
    char* test_string = "t";
    init_interpreter(256);
    lisp_object_t obj = parse1(&test_string);
    check(obj == T, "is t");
    char* result = print_object(obj);
    check(strcmp("t", result) == 0, "print t");
    free(result);
    free_interpreter();
}

static void test_strings()
{
    test_name = "strings";
    init_interpreter(256);
    lisp_object_t s1 = allocate_string(6, "hello");
    lisp_object_t s2 = allocate_string(6, "hello");
    lisp_object_t s3 = allocate_string(7, "oohaah");
    check(string_equalp(s1, s2) == T, "equal strings are equalp/1");
    check(string_equalp(s2, s1) == T, "equal strings are equalp/2");
    check(string_equalp(s1, s3) == NIL, "unequal strings are not equalp/1");
    check(string_equalp(s2, s3) == NIL, "unequal strings are not equalp/2");
    size_t len;
    char* str;
    get_string_parts(s1, &len, &str);
    check(len == 5, "get_string_parts/length");
    check(strncmp("hello", str, 5) == 0, "get_string_parts/string");
    free_interpreter();
}

static void test_print_empty_cons()
{
    test_name = "print_empty_cons";
    init_interpreter(256);
    lisp_object_t empty = cons(NIL, NIL);
    char* str = print_object(empty);
    check(strcmp("(nil)", str) == 0, "(nil)");
    free(str);
    free_interpreter();
}

static void test_symbol_pointer()
{
    test_name = "symbol_pointer";
    lisp_object_t obj_without_tag = 8;
    lisp_object_t tagged_obj = obj_without_tag | SYMBOL_TYPE;
    struct symbol* ptr = SymbolPtr(tagged_obj);
    check((unsigned long)obj_without_tag == (unsigned long)ptr, "correct pointer");
}

static void test_parse_symbol()
{
    test_name = "parse_symbol";
    char* test_string = "foo";
    init_interpreter(256);
    lisp_object_t result = parse1(&test_string);
    check(symbolp(result) == T, "symbolp");
    check(consp(result) == NIL, "not consp");
    char* str = print_object(result);
    check(strcmp("foo", str) == 0, "print");
    free(str);
    free_interpreter();
}

static void test_parse_multiple_symbols()
{
    test_name = "parse_multiple_symbols";
    char* s1 = "foo";
    init_interpreter(1024);
    interp->symbol_table = NIL;
    lisp_object_t sym1 = parse1(&s1);
    char* s2 = "bar";
    lisp_object_t sym2 = parse1(&s2);
    char* str = print_object(interp->symbol_table);
    check(strcmp("(bar foo)", str) == 0, "symbol table looks right");
    free(str);
    char* s3 = "bar";
    lisp_object_t sym3 = parse1(&s3);
    check(eq(sym2, sym3) == T, "symbols eq");
    str = print_object(interp->symbol_table);
    check(strcmp("(bar foo)", str) == 0, "symbol table looks right(2)");
    free(str);
    free_interpreter();
}

static void test_parse_list_of_symbols()
{
    test_name = "parse_list_of_symbols";
    char* test_string = "(hello you are nice)";
    init_interpreter(16384);
    lisp_object_t result = parse1(&test_string); //bad
    check(consp(result) != NIL, "consp");
    check(symbolp(car((result))) != NIL, "first symbolp");
    char* str = print_object(result);
    check(strcmp("(hello you are nice)", str) == 0, "prints ok");
    free(str);
    free_interpreter();
}

static void test_parse_string()
{
    test_name = "parse_string";
    init_interpreter(256);
    char* string = "\"hello\"";
    lisp_object_t obj = parse_string(&string);
    check(stringp(obj), "stringp");
    size_t len;
    char* str;
    get_string_parts(obj, &len, &str);
    check(len == 5, "length");
    check(strcmp("hello", str) == 0, "value");
    free_interpreter();
}

static void test_parse_string_with_escape_characters()
{
    test_name = "test_parse_string_with_escape_characters";
    init_interpreter(256);
    char* string = "\"he\\\"llo\\n\\t\\r\"";
    lisp_object_t obj = parse_string(&string);
    check(stringp(obj), "stringp");
    size_t len;
    char* str;
    get_string_parts(obj, &len, &str);
    check(len == 9, "length");
    check(strcmp("he\"llo\n\t\r", str) == 0, "value");
    free_interpreter();
}

static void test_eq()
{
    test_name = "eq";
    check(eq(0, 0) == T, "(eq 0 0) is T");
    check(eq(1, 1) == T, "(eq 1 1) is T");
    check(eq(NIL, NIL) == T, "(eq nil nil) is T");
    check(eq(T, T) == T, "(eq t t) is T");
    check(eq(0, 0) != NIL, "(eq 0 0) is not NIL");
    check(eq(1, 1) != NIL, "(eq 1 1) is not NIL");
    check(eq(NIL, NIL) != NIL, "(eq nil nil) is not NIL");
    check(eq(T, T) != NIL, "(eq t t) is not NIL");
}

/* The parse updates the pointer passed to it.
   This test is to say that we think this is OK */
static void test_parser_advances_pointer()
{
    test_name = "parser_advances_pointer";
    char* s1 = "foo";
    char* before = s1;
    init_interpreter(256);
    lisp_object_t sym1 = parse1(&s1);
    check(s1 - before == 3, "pointer advanced");
    free_interpreter();
}

static void test_parse_multiple_objects_callback(void* data, lisp_object_t obj)
{
    print_object_to_buffer(obj, (struct string_buffer*)data);
}

void test_parse_multiple_objects()
{
    test_name = "parse_multiple_objects";
    char* test_string = "foo bar";
    init_interpreter(256);
    struct string_buffer sb;
    string_buffer_init(&sb);
    parse(test_string, test_parse_multiple_objects_callback, (void*)&sb);
    char* str = string_buffer_to_string(&sb);
    string_buffer_free_links(&sb);
    check(strcmp("foobar", str) == 0, "parses both symbols");
    free(str);
    free_interpreter();
}

static void test_vector_initialization()
{
    init_interpreter(1024);
    lisp_object_t v = allocate_vector(3);
    check(eq(svref(v, 0), NIL) != NIL, "first element nil");
    check(eq(svref(v, 1), NIL) != NIL, "second element nil");
    check(eq(svref(v, 2), NIL) != NIL, "third element nil");
    free_interpreter();
}

static void test_vector_svref()
{
    test_name = "vector_svref";
    init_interpreter(1024);
    char* symbol_text = "foo";
    lisp_object_t sym = parse1(&symbol_text);
    lisp_object_t v = allocate_vector(3);
    char* list_text = "(a b c)";
    lisp_object_t list = parse1(&list_text);
    svref_set(v, 0, 14);
    svref_set(v, 1, sym);
    svref_set(v, 2, list);
    check(eq(svref(v, 0), 14) != NIL, "first element");
    check(eq(svref(v, 1), sym) != NIL, "second element");
    check(eq(svref(v, 2), list) != NIL, "third element");
    free_interpreter();
}

static void test_parse_vector()
{
    test_name = "parse_vector";
    char* text = "#(a b c)";
    init_interpreter(1024);
    lisp_object_t result = parse1(&text);
    check(vectorp(result) == T, "vectorp");
    char* a_text = "a";
    lisp_object_t sym_a = parse1(&a_text);
    check(eq(sym_a, svref(result, 0)) == T, "first element");
    char* b_text = "b";
    lisp_object_t sym_b = parse1(&b_text);
    check(eq(sym_b, svref(result, 1)) == T, "second element");
    char* c_text = "c";
    lisp_object_t sym_c = parse1(&c_text);
    check(eq(sym_c, svref(result, 2)) == T, "third element");
    free_interpreter();
}

static void test_print_vector()
{
    test_name = "print_vector";
    char* text = "#(a b c)";
    init_interpreter(1024);
    lisp_object_t result = parse1(&text);
    char* str = print_object(result);
    check(strcmp("#(a b c)", str) == 0, "correct string");
    free(str);
    free_interpreter();
}

static void test_car_of_nil()
{
    test_name = "car_of_nil";
    check(car(NIL) == NIL, "car of nil is nil");
}

static void test_cdr_of_nil()
{
    test_name = "cdr_of_nil";
    check(cdr(NIL) == NIL, "cdr of nil is nil");
}

static void test_parse_list_of_dotted_pairs()
{
    test_name = "parse_list_of_dotted_pairs";
    init_interpreter(4096);
    char* text1 = "((X . SHAKESPEARE) (Y . (THE TEMPEST)))";
    lisp_object_t obj = parse1(&text1);
    char* str = print_object(obj);
    check(strcmp("((X . SHAKESPEARE) (Y THE TEMPEST))", str) == 0, "");
    free(str);
    free_interpreter();
}

static void test_sublis()
{
    test_name = "test_sublis";
    init_interpreter(4096);
    char* text1 = "((X . SHAKESPEARE) (Y . (THE TEMPEST)))";
    char* text2 = "(X WROTE Y)";
    lisp_object_t obj1 = parse1(&text1);
    lisp_object_t obj2 = parse1(&text2);
    lisp_object_t result = sublis(obj1, obj2);
    char* str = print_object(result);
    check(strcmp("(SHAKESPEARE WROTE (THE TEMPEST))", str) == 0, "");
    free(str);
    free_interpreter();
}

static void test_null()
{
    test_name = "null";
    check(null(NIL) != NIL, "nil");
    check(null(T) == NIL, "t");
}

static void test_append()
{
    test_name = "append";
    init_interpreter(4096);
    char* text1 = "(A B)";
    char* text2 = "(C D E)";
    lisp_object_t obj1 = parse1(&text1);
    lisp_object_t obj2 = parse1(&text2);
    lisp_object_t result = append(obj1, obj2);
    char* str = print_object(result);
    check(strcmp("(A B C D E)", str) == 0, "");
    free(str);
    free_interpreter();
}

static void test_member()
{
    test_name = "member";
    init_interpreter(4096);
    char* text1 = "A";
    char* text2 = "X";
    char* text3 = "(A B C D)";
    lisp_object_t obj1 = parse1(&text1);
    lisp_object_t obj2 = parse1(&text2);
    lisp_object_t obj3 = parse1(&text3);
    check(member(obj1, obj3) != NIL, "A is member");
    check(member(obj2, obj3) == NIL, "X is not member");
    free_interpreter();
}

static void test_assoc()
{
    test_name = "assoc";
    init_interpreter(4096);
    char* text1 = "((A . (M N)) (B . (CAR X)) (C . (QUOTE M)) (C . (CDR x)))";
    char* text2 = "B";
    char* text3 = "X";
    lisp_object_t alist = parse1(&text1);
    lisp_object_t b = parse1(&text2);
    lisp_object_t x = parse1(&text3);
    lisp_object_t result = assoc(b, alist);
    char* str = print_object(result);
    check(strcmp("(B CAR X)", str) == 0, "match found");
    free(str);
    check(assoc(x, alist) == NIL, "match not present");
    free_interpreter();
}

static void test_pairlis()
{
    test_name = "pairlis";
    init_interpreter(4096);
    char* text1 = "(A B C)";
    char* text2 = "(U V W)";
    char* text3 = "((D . X) (E . Y))";
    lisp_object_t x = parse1(&text1);
    lisp_object_t y = parse1(&text2);
    lisp_object_t a = parse1(&text3);
    lisp_object_t result = pairlis(x, y, a);
    char* str = print_object(result);
    check(strcmp("((A . U) (B . V) (C . W) (D . X) (E . Y))", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_sym()
{
    test_name = "sym";
    init_interpreter(1024);
    lisp_object_t x1 = sym("x");
    lisp_object_t x2 = sym("x");
    lisp_object_t y = sym("y");
    check(eq(x1, x2) != NIL, "(eq x1 x2)");
    check(eq(x1, y) == NIL, "(not (eq x1 y))");
    check(eq(x2, y) == NIL, "(not (eq x2 y))");
    free_interpreter();
}

static void test_evalquote_helper(char* fnstr, char* exprstr, char* expected)
{
    init_interpreter(4096);
    char* fnstr_copy = fnstr;
    lisp_object_t fn = parse1(&fnstr);
    lisp_object_t expr = parse1(&exprstr);
    lisp_object_t result = evalquote(fn, expr);
    char* result_str = print_object(result);
    check(strcmp(expected, result_str) == 0, fnstr_copy);
    free(result_str);
    free_interpreter();
}

static void test_evalquote()
{
    test_name = "evalquote";
    test_evalquote_helper("CAR", "((A . B))", "A");
    test_evalquote_helper("CDR", "((A . B))", "B");
    test_evalquote_helper("CDR", "((A . B))", "B");
    test_evalquote_helper("ATOM", "(A)", "t");
    test_evalquote_helper("ATOM", "((A . B))", "nil");
    test_evalquote_helper("EQ", "(A A)", "t");
    test_evalquote_helper("EQ", "(A B)", "nil");
}

static lisp_object_t test_eval_string_helper(char* exprstr)
{
    lisp_object_t expr = parse1(&exprstr);
    lisp_object_t result = eval_toplevel(expr);
    return result;
}

static void test_eval_helper(char* exprstr, char* expectedstr)
{
    init_interpreter(4096);
    char* exprstr_save = exprstr;
    lisp_object_t result = test_eval_string_helper(exprstr);
    char* resultstr = print_object(result);
    struct string_buffer sb;
    string_buffer_init(&sb);
    string_buffer_append(&sb, exprstr_save);
    string_buffer_append(&sb, " => ");
    string_buffer_append(&sb, expectedstr);
    int ok = strcmp(expectedstr, resultstr) == 0;
    if (!ok) {
        string_buffer_append(&sb, " ACTUAL => ");
        string_buffer_append(&sb, resultstr);
    }
    char* stuff = string_buffer_to_string(&sb);
    check(ok, stuff);
    free(stuff);
    free(resultstr);
    string_buffer_free_links(&sb);
    free_interpreter();
}

static void test_eval()
{
    test_name = "eval";
    test_eval_helper("t", "t");
    test_eval_helper("3", "3");
    test_eval_helper("(CONS (QUOTE A) (QUOTE B))", "(A . B)");
    test_eval_helper("(COND ((EQ (CAR (CONS (QUOTE A) NIL)) (QUOTE A)) (QUOTE OK)))", "OK");
    test_eval_helper("(COND ((EQ (CAR (CONS (QUOTE A) NIL)) (QUOTE B)) (QUOTE BAD)) (t (QUOTE OK)))", "OK");
    test_eval_helper("((LAMBDA (X) (CAR X)) (CONS (QUOTE A) (QUOTE B)))", "A");
}

static void test_defun()
{
    test_name = "defun";
    init_interpreter(4096);
    lisp_object_t result1 = test_eval_string_helper("(DEFUN FOO (X) (CONS X (QUOTE BAR)))");
    lisp_object_t result2 = test_eval_string_helper("(FOO 14)");
    char* str = print_object(result2);
    check(strcmp("(14 . BAR)", str) == 0, "result");
    free(str);
    free_interpreter();
}

static void test_load()
{
    test_name = "load";
    init_interpreter(32768);
    lisp_object_t result1 = test_eval_string_helper("(LOAD \"/home/graham/toy-lisp-interpreter/test-load.lisp\")");
    check(result1 == T, "load returns T");
    lisp_object_t result2 = test_eval_string_helper("(TEST1 (QUOTE THERE))");
    char* str = print_object(result2);
    check(strcmp("(HELLO . THERE)", str) == 0, "result of TEST1");
    free(str);
    free_interpreter();
}

int main(int argc, char** argv)
{
    test_skip_whitespace();
    test_parse_integer();
    test_parse_negative_integer();
    test_parse_single_integer_list();
    test_parse_integer_list();
    test_parse_dotted_pair_of_integers();
    test_string_buffer();
    test_print_integer();
    test_print_single_integer_list();
    test_print_integer_list();
    test_print_dotted_pair();
    test_print_complex_list();
    test_nil_is_not_a_cons();
    test_nil_is_a_symbol();
    test_t_is_not_a_cons();
    test_t_is_a_symbol();
    test_read_and_print_nil();
    test_read_and_print_t();
    test_strings();
    test_print_empty_cons();
    test_symbol_pointer();
    test_eq();
    test_parse_symbol();
    test_parse_multiple_symbols();
    test_parse_list_of_symbols();
    test_parse_string();
    test_parse_string_with_escape_characters();
    test_parser_advances_pointer();
    test_parse_multiple_objects();
    test_vector_initialization();
    test_vector_svref();
    test_parse_vector();
    test_print_vector();
    test_car_of_nil();
    test_cdr_of_nil();
    test_parse_list_of_dotted_pairs();
    test_sublis();
    test_null();
    test_append();
    test_member();
    test_assoc();
    test_pairlis();
    test_sym();
    test_evalquote();
    test_eval();
    test_defun();
    test_load();
    return 0;
}
