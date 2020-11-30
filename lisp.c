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

/* Might be nice to have a lisp_memory struct separate from the interpreter */
struct lisp_interpreter {
    lisp_object_t* heap;
    lisp_object_t* next_free;
    lisp_object_t symbol_table; /* A root for GC */
    size_t heap_size_bytes;
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
    /* This is a Lisp integer so >> 3 to get C value */
    lisp_object_t len;
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
    check_cons(obj);
    return ConsPtr(obj)->car;
}

lisp_object_t cdr(lisp_object_t obj)
{
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

static lisp_object_t* check_vector_bounds_get_storage(lisp_object_t vector, lisp_object_t index)
{
    check_vector(vector);
    struct vector* v = VectorPtr(vector);
    if (index >= v->len) {
        printf("Index %lu out of bounds for vector (len=%lu)\n", index >> 3, v->len >> 3);
        abort();
    }
    lisp_object_t* storage = (lisp_object_t*)v->storage;
    return storage;
}

lisp_object_t svref(lisp_object_t vector, lisp_object_t index)
{
    lisp_object_t* storage = check_vector_bounds_get_storage(vector, index);
    return storage[index >> 3];
}

lisp_object_t svref_set(lisp_object_t vector, lisp_object_t index, lisp_object_t newvalue)
{
    lisp_object_t* storage = check_vector_bounds_get_storage(vector, index);
    int i = index >> 3;
    storage[i] = newvalue;
    return newvalue;
}

lisp_object_t allocate_vector(size_t size)
{
    /* Allocate header */
    struct vector* v = (struct vector*)allocate_lisp_objects(2);
    v->len = size << 3;
    /* Allocate storage */
    v->storage = allocate_lisp_objects(size);
    lisp_object_t result = (lisp_object_t)v | VECTOR_TYPE;
    for (int i = 0; i < size; i++)
        svref_set(result, i << 3, NIL);
    return result;
}

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
    *lenptr = *name_length_ptr;
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
        lisp_object_t lisp_string = allocate_string(len, tmp);
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
    while (strchr("\r\n\t ", **text) != NULL)
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
    } else if (**text == ')') {
        (*text)++;
        rplacd(new_cons, NIL);
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
        return v->len >> 3; /* Return a C int */
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
        svref_set(vector, i << 3, car(c));
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

lisp_object_t parse1(char** text)
{
    skip_whitespace(text);
    if (!**text)
        abort();
    if (**text == '(') {
        (*text)++;
        return parse_cons(text);
    } else if (**text == ')') {
        printf("Unexpected close paren\n");
    } else if (**text == '-' || (**text >= '0' && **text <= '9')) {
        return parse_integer(text) << 3;
    } else if (**text == '#') {
        return parse_dispatch(text);
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

/* Printing */

/* Maybe this string buffer stuff could be rewritten to use Lisp objects */
struct string_buffer_link {
    char* string;
    struct string_buffer_link* next;
    size_t len;
};

struct string_buffer {
    struct string_buffer_link* head;
    struct string_buffer_link* tail;
    size_t len;
};

void string_buffer_append(struct string_buffer* sb, char* string)
{
    struct string_buffer_link* link = malloc(sizeof(struct string_buffer_link));
    /* Update links */
    link->next = NULL;
    if (sb->tail)
        sb->tail->next = link;
    sb->tail = link;
    if (!sb->head)
        sb->head = link;
    /* Update lengths */
    link->len = strlen(string);
    sb->len += link->len;
    /* Copy string */
    link->string = malloc(link->len + 1);
    strcpy(link->string, string);
}

void string_buffer_init(struct string_buffer* sb)
{
    sb->head = NULL;
    sb->tail = NULL;
    sb->len = 0;
}

/* Caller is responsible for freeing returned memory */
char* string_buffer_to_string(struct string_buffer* sb)
{
    char* result = malloc(sb->len + 1);
    char* cur = result;
    for (struct string_buffer_link* link = sb->head; link; link = link->next) {
        strcpy(cur, link->string);
        cur += link->len;
    }
    return result;
}

void string_buffer_free_link(struct string_buffer_link*);

void string_buffer_free_links(struct string_buffer* sb)
{
    if (sb->head)
        string_buffer_free_link(sb->head);
}

void string_buffer_free_link(struct string_buffer_link* link)
{
    if (link->next)
        string_buffer_free_link(link->next);
    free(link->string);
    free(link);
}

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
            print_object_to_buffer(svref(obj, i << 3), sb);
            string_buffer_append(sb, " ");
        }
        print_object_to_buffer(svref(obj, (len - 1) << 3), sb);
        string_buffer_append(sb, ")");
    }
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
    lisp_object_t s1 = allocate_string(5, "hello");
    lisp_object_t s2 = allocate_string(5, "hello");
    lisp_object_t s3 = allocate_string(6, "oohaah");
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
    svref_set(v, 0, 14 << 3);
    svref_set(v, 1 << 3, sym);
    svref_set(v, 2 << 3, list);
    check(eq(svref(v, 0), 14 << 3) != NIL, "first element");
    check(eq(svref(v, 1 << 3), sym) != NIL, "second element");
    check(eq(svref(v, 2 << 3), list) != NIL, "third element");
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
    check(eq(sym_b, svref(result, 1 << 3)) == T, "second element");
    char* c_text = "c";
    lisp_object_t sym_c = parse1(&c_text);
    check(eq(sym_c, svref(result, 2 << 3)) == T, "third element");
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
    test_parser_advances_pointer();
    test_parse_multiple_objects();
    test_vector_initialization();
    test_vector_svref();
    test_parse_vector();
    test_print_vector();
    return 0;
}
