#include "lisp.h"
#include "string_buffer.h"

#include <alloca.h>
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
};

struct vector {
    size_t len;
    lisp_object_t storage;
};

lisp_object_t istype(lisp_object_t obj, int type);

static void check_type(lisp_object_t obj, int type)
{
    static char *type_names[5] = { "integer", "symbol", "cons", "string", "vector" };
    if (istype(obj, type) == NIL) {
        char *obj_string = print_object(obj);
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

void init_interpreter(size_t heap_size)
{
    interp = (struct lisp_interpreter *)malloc(sizeof(struct lisp_interpreter));
    if (sizeof(lisp_object_t) != sizeof(void *))
        abort();
    interp->heap_size_bytes = heap_size * sizeof(lisp_object_t);
    interp->heap = mmap((void *)0x100000000000, interp->heap_size_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (interp->heap == (lisp_object_t *)-1) {
        perror("init_interpreter: mmap failed");
        exit(1);
    }
    bzero(interp->heap, interp->heap_size_bytes);
    interp->next_free = interp->heap;
    cons_heap_init(&interp->cons_heap, 96);
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
    interp->environ = NIL;
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
    struct cons *p = NULL;
    for (int i = 0; i < size - 1; i++) {
        p = cons_heap->actual_heap + i;
        p->cdr = (lisp_object_t)(p + 1);
    }
    for (int i = 0; i < size; i++) {
        p = cons_heap->actual_heap + i;
        p->mark_bit = 0;
        p->is_allocated = 0;
        p->car = NIL;
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
    if (obj == NIL || obj == T)
        return;
    else if (consp(obj) == T) {
        struct cons *cons_ptr = ConsPtr(obj);
        cons_ptr->mark_bit = 1;
        mark_object(cons_ptr->car);
        if (!cons_ptr->cdr)
            abort();
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
    if (top_of_stack) {
        for (lisp_object_t *s = top_of_stack; s > (lisp_object_t *)rbp; s--) {
            if (consp(*s)) {
                struct cons *p = ConsPtr(*s);
                struct cons *cons_heap_limit = cons_heap->actual_heap + cons_heap->size;
                if (p >= cons_heap->actual_heap && p < cons_heap_limit && p->is_allocated)
                    mark_object(*s);
            }
        }
    } else
        abort();
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
        if (p->mark_bit && !p->is_allocated)
            abort();
        if (p->is_allocated && !p->mark_bit) {
            p->cdr = (lisp_object_t)cons_heap->free_list_head;
            p->is_allocated = 0;
            cons_heap->free_list_head = p;
            cons_heap->allocation_count--;
        } else
            p->mark_bit = 0;
    }
}

static void gc(struct cons_heap *cons_heap)
{
    size_t before = cons_heap->allocation_count;
    mark(cons_heap);
    sweep(cons_heap);
    size_t nfreed = before - cons_heap->allocation_count;
    printf("Garbage collection: %lu conses freed\n", nfreed);
}

lisp_object_t cons_heap_allocate_cons(struct cons_heap *cons_heap)
{
    if (!cons_heap->free_list_head)
        gc(cons_heap);
    if (!cons_heap->free_list_head)
        abort();
    struct cons *the_cons = cons_heap->free_list_head;
    cons_heap->free_list_head = (struct cons *)the_cons->cdr;
    the_cons->car = NIL;
    the_cons->cdr = NIL;
    the_cons->is_allocated = 1;
    if (the_cons->mark_bit)
        abort();
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
        lisp_object_t obj = allocate_lisp_objects(3);
        struct symbol *s = (struct symbol *)obj;
        s->name = name;
        s->value = NIL;
        s->function = NIL;
        lisp_object_t symbol = obj | SYMBOL_TYPE;
        interp->symbol_table = cons(symbol, interp->symbol_table);
        return symbol;
    }
}

lisp_object_t parse_symbol(char **text)
{
    static char *delimiters = " \n\t\r)\0";
    char *p = *text;
    while (!strchr(delimiters, *p))
        p++;
    size_t len = p - *text;
    char *tmp = alloca(len + 1);
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

int parse_integer(char **text)
{
    char *start = *text;
    size_t len = 0;
    for (; **text == '-' || (**text >= '0' && **text <= '9'); (*text)++, len++)
        ;
    char *tmp = (char *)alloca(len + 1);
    strncpy(tmp, start, len);
    tmp[len] = 0;
    return atoi(tmp);
}

void skip_whitespace(char **text)
{
    while (**text && strchr("\r\n\t ", **text) != NULL)
        (*text)++;
}

lisp_object_t parse_cons(char **text)
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

lisp_object_t parse_vector(char **text)
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

lisp_object_t parse_dispatch(char **text)
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

lisp_object_t parse1(char **text)
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

void parse(char *text, void (*callback)(void *, lisp_object_t), void *callback_data)
{
    char **cursor = &text;
    while (*text)
        callback(callback_data, parse1(cursor));
}

/* Convenience function */
lisp_object_t sym(char *string)
{
    char *tmp = (char *)malloc(strlen(string) + 1);
    char *tmp_save = tmp;
    strcpy(tmp, string);
    lisp_object_t result = parse_symbol(&tmp);
    free(tmp_save);
    return result;
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

void debug_obj(lisp_object_t obj)
{
    char *str = print_object(obj);
    printf("%s", str);
    free(str);
}

void debug_obj2(char *msg, lisp_object_t obj)
{
    printf("%s: ", msg);
    debug_obj(obj);
    printf("\n");
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
        int value = obj >> 3;
        int length = snprintf(NULL, 0, "%d", value);
        char *str = alloca(length + 1);
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
    }
}

lisp_object_t parse_string(char **text)
{
    if (**text != '"')
        abort();
    (*text)++;
    int len = 0;
    int escaped = 0;
    struct string_buffer sb;
    string_buffer_init(&sb);
    for (; escaped || **text != '"'; (*text)++) {
        char *p = *text;
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
            char *tmp = (char *)alloca(2);
            tmp[0] = c;
            tmp[1] = '\0';
            string_buffer_append(&sb, tmp);
            len++;
        }
    }
    if (**text != '"')
        /* No closing " */
        abort();
    (*text)++; /* Move past closing " */
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

lisp_object_t eval_toplevel(lisp_object_t e)
{
    return eval(e, interp->environ);
}

static void load_eval_callback(void *ignored, lisp_object_t obj)
{
    lisp_object_t result = eval_toplevel(obj);
    char *str = print_object(result);
    printf("; %s\n", str);
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
    FILE *f = fopen(str, "r");
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
    char *text = string_buffer_to_string(&sb);
    parse(text, load_eval_callback, NULL);
    free(text);
    string_buffer_free_links(&sb);
}

lisp_object_t apply(lisp_object_t fn, lisp_object_t x, lisp_object_t a)
{
    if (atom(fn) != NIL) {
        struct symbol *sym = SymbolPtr(fn);
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

lisp_object_t eval(lisp_object_t e, lisp_object_t a)
{
    if (e == NIL || e == T || integerp(e) != NIL || vectorp(e) != NIL || stringp(e) != NIL)
        return e;
    if (atom(e) != NIL)
        return cdr(assoc(e, a));
    else if (atom(car(e) != NIL))
        if (eq(car(e), interp->syms.quote) != NIL)
            return car(cdr(e));
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
