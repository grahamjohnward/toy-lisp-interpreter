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
#define INTEGER_TYPE 0
#define SYMBOL_TYPE 1
#define CONS_TYPE 2
#define STRING_TYPE 3

#define ConsPtr(obj) ((cons*)((obj) ^ TYPE_MASK))

/* Might be nice to have a lisp_memory struct separate from the interpreter */
typedef struct lisp_interpreter_t {
    lisp_object_t* heap;
    lisp_object_t* next_free;
    lisp_object_t symbol_table;
} lisp_interpreter_t;

typedef struct {
    lisp_object_t car;
    lisp_object_t cdr;
} cons;

char* print_object(lisp_object_t obj);

static void check_cons(lisp_object_t cons)
{
    if ((cons & TYPE_MASK) != CONS_TYPE) {
        char* obj_string = print_object(cons);
        printf("Not a cons: %s\n", obj_string);
        free(obj_string);
        exit(1);
    }
}

lisp_object_t rplaca(lisp_object_t the_cons, lisp_object_t the_car)
{
    check_cons(the_cons);
    cons* p = ConsPtr(the_cons);
    p->car = the_car;
    return the_cons;
}

lisp_object_t rplacd(lisp_object_t the_cons, lisp_object_t the_cdr)
{
    check_cons(the_cons);
    cons* p = ConsPtr(the_cons);
    p->cdr = the_cdr;
    return the_cons;
}

lisp_object_t allocate_cons(lisp_interpreter_t* interp)
{
    lisp_object_t new_cons = (lisp_object_t)interp->next_free;
    interp->next_free += 2;
    new_cons |= CONS_TYPE;
    rplaca(new_cons, NIL);
    rplacd(new_cons, NIL);
    return new_cons;
}

static void init_interpreter(lisp_interpreter_t* interp, size_t heap_size)
{
    if (sizeof(lisp_object_t) != sizeof(void*))
        abort();
    interp->heap = mmap(NULL, heap_size, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (interp->heap == (lisp_object_t*)-1) {
        perror("mmap failed");
        exit(1);
    }
    bzero(interp->heap, heap_size);
    interp->next_free = interp->heap;
    interp->symbol_table = allocate_cons(interp);
}

lisp_object_t allocate_single_object(lisp_interpreter_t* interp)
{
    return (lisp_object_t)interp->next_free++;
}

lisp_object_t allocate_string(lisp_interpreter_t* interp, size_t len, char* str)
{
    lisp_object_t obj = (lisp_object_t)interp->next_free;
    size_t* header_address = (size_t*)obj;
    *header_address = len;
    char* straddr = (char*)(header_address + 1);
    strncpy(straddr, str, len);
    interp->next_free = (lisp_object_t*)(straddr + len + 8 - len % sizeof(lisp_object_t));
    return obj | STRING_TYPE;
}

lisp_object_t parse_symbol(lisp_interpreter_t* interp, char** text)
{
    static char* delimiters = " \n\t\r)\0";
    char* p = *text;
    for (; !strchr(delimiters, *p); p++)
        ;
    size_t len = p - *text;
    char* tmp = alloca(len);
    strcpy(tmp, *text);
    *text += len;
    if (strcmp(tmp, "nil") == 0) {
        return NIL;
    } else if (strcmp(tmp, "t") == 0) {
        return T;
    } else {
        /* Create a Lisp string on the heap */
        lisp_object_t lisp_string = allocate_string(interp, len, tmp);
        /* Look for a matching string in the symbol table */
        /* Symbol table is a Lisp assoc list */
        /* If found, return */
        /* Else, create new symbol and return */
        return NIL;
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

lisp_object_t parse1(lisp_interpreter_t*, char**);

lisp_object_t parse_cons(lisp_interpreter_t* interp, char** text)
{
    skip_whitespace(text);
    lisp_object_t new_cons = allocate_cons(interp);
    rplaca(new_cons, parse1(interp, text));
    skip_whitespace(text);
    if (**text == '.') {
        (*text)++;
        skip_whitespace(text);
        rplacd(new_cons, parse1(interp, text));
    } else if (**text == ')') {
        (*text)++;
        rplacd(new_cons, NIL);
    } else {
        rplacd(new_cons, parse_cons(interp, text));
    }
    return new_cons;
}

lisp_object_t parse1(lisp_interpreter_t* interp, char** text)
{
    skip_whitespace(text);
    if (!**text)
        abort();
    if (**text == '(') {
        (*text)++;
        return parse_cons(interp, text);
    } else if (**text == ')') {
        printf("Unexpected close paren\n");
    } else if (**text == '-' || (**text >= '0' && **text <= '9')) {
        return parse_integer(text) << 3;
    } else {
        lisp_object_t sym = parse_symbol(interp, text);
        return sym;
    }
    return 0;
}

void parse(lisp_interpreter_t* interp, char* text,
    void (*callback)(lisp_object_t))
{
    char** cursor = &text;
    while (*text)
        callback(parse1(interp, cursor));
}

lisp_object_t car(lisp_object_t obj)
{
    check_cons(obj);
    cons* foo = ConsPtr(obj);
    return ConsPtr(obj)->car;
}

lisp_object_t cdr(lisp_object_t obj)
{
    check_cons(obj);
    return ConsPtr(obj)->cdr;
}

lisp_object_t consp(lisp_object_t obj)
{
    if ((obj & TYPE_MASK) == CONS_TYPE) {
        return T;
    } else {
        return NIL;
    }
}

lisp_object_t symbolp(lisp_object_t obj)
{
    if ((obj & TYPE_MASK) == SYMBOL_TYPE) {
        return T;
    } else {
        return NIL;
    }
}

lisp_object_t integerp(lisp_object_t obj)
{
    if ((obj & TYPE_MASK) == INTEGER_TYPE) {
        return T;
    } else {
        return NIL;
    }
}

typedef struct string_buffer_link {
    char* string;
    struct string_buffer_link* next;
    size_t len;
} string_buffer_link;

typedef struct {
    string_buffer_link* head;
    string_buffer_link* tail;
    size_t len;
} string_buffer_t;

void string_buffer_append(string_buffer_t* sb, char* string)
{
    string_buffer_link* link = malloc(sizeof(string_buffer_link));
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
    link->string = malloc(link->len);
    strcpy(link->string, string);
}

void string_buffer_print(string_buffer_link* sb)
{
    for (string_buffer_link* link = sb; link; link = link->next)
        printf("%s", link->string);
}

void string_buffer_init(string_buffer_t* sb)
{
    sb->head = NULL;
    sb->tail = NULL;
    sb->len = 0;
}

/* Caller is responsible for freeing returned memory */
char* string_buffer_to_string(string_buffer_t* sb)
{
    char* result = malloc(sb->len);
    char* cur = result;
    for (string_buffer_link* link = sb->head; link; link = link->next) {
        strcpy(cur, link->string);
        cur += link->len;
    }
    return result;
}

void string_buffer_free_link(string_buffer_link*);

/* N.B. does not actually free the buffer struct itself */
void string_buffer_free_links(string_buffer_t* sb)
{
    string_buffer_free_link(sb->head);
}

void string_buffer_free_link(string_buffer_link* link)
{
    if (link->next)
        string_buffer_free_link(link->next);
    free(link->string);
    free(link);
}

void print_object_to_buffer(lisp_object_t, string_buffer_t*);

char* print_object(lisp_object_t obj)
{
    string_buffer_t sb;
    string_buffer_init(&sb);
    print_object_to_buffer(obj, &sb);
    char* result = string_buffer_to_string(&sb);
    string_buffer_free_links(&sb);
    return result;
}

void print_cons_to_buffer(lisp_object_t, string_buffer_t*);

void print_cons_to_buffer(lisp_object_t obj, string_buffer_t* sb)
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

void print_object_to_buffer(lisp_object_t obj, string_buffer_t* sb)
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
    } else if (consp(obj)) {
        string_buffer_append(sb, "(");
        print_cons_to_buffer(obj, sb);
        string_buffer_append(sb, ")");
    } else {
        abort();
    }
}

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
    lisp_interpreter_t interp;
    init_interpreter(&interp, 256);
    lisp_object_t result = parse1(&interp, &test_string);
    check(consp(result), "consp");
    lisp_object_t result_car = car(result);
    check(integerp(result_car), "car is int");
    check(result_car == 14 << 3, "car value");
    lisp_object_t result_cdr = cdr(result);
    check(NIL == result_cdr, "cdr is null");
}

static void test_parse_integer_list()
{
    test_name = "parse_integer_list";
    char* test_string = "(23 71)";
    lisp_interpreter_t interp;
    init_interpreter(&interp, 256);
    lisp_object_t result = parse1(&interp, &test_string);
    check(consp(result), "consp");
    lisp_object_t result_car = car(result);
    check(integerp(result_car), "car is int");
    check(result_car == 23 << 3, "car value");
    lisp_object_t result_cdr = cdr(result);
    cons* frobbo = (cons*)result;
    check(NIL != result_cdr, "cdr is not null");
    check(consp(result_cdr), "cdr is a pair");
    lisp_object_t cadr = car(result_cdr);
    check(integerp(cadr), "cadr is int");
    check(cadr == 71 << 3, "cadr value");
}

static void test_parse_dotted_pair_of_integers()
{
    test_name = "parse_dotted_pair_of_integers";
    char* test_string = "(45 . 123)";
    lisp_interpreter_t interp;
    init_interpreter(&interp, 256);
    lisp_object_t result = parse1(&interp, &test_string);
    check(consp(result), "consp");
    check(integerp(car(result)), "car is int");
    check(integerp(cdr(result)), "cdr is int");
    check(car(result) == 45 << 3, "car value");
    check(cdr(result) == 123 << 3, "cdr value");
}

static void test_parse_symbol()
{
    test_name = "test_parse_symbol";
    char* test_string = "foo";
    lisp_interpreter_t interp;
    init_interpreter(&interp, 256);
    lisp_object_t result = parse1(&interp, &test_string);
}

static void test_string_buffer()
{
    test_name = "test_string_buffer";
    string_buffer_t sb;
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
    test_name = "test_print_integer";
    char* test_string = "93";
    lisp_interpreter_t interp;
    init_interpreter(&interp, 256);
    lisp_object_t obj = parse1(&interp, &test_string);
    char* result = print_object(obj);
    check(strcmp("93", result) == 0, "string value");
    free(result);
}

static void test_print_single_integer_list()
{
    test_name = "test_print_single_integer_list";
    char* test_string = "(453)";
    lisp_interpreter_t interp;
    init_interpreter(&interp, 256);
    lisp_object_t obj = parse1(&interp, &test_string);
    char* result = print_object(obj);
    check(strcmp("(453)", result) == 0, "string value");
    free(result);
}

static void test_print_integer_list()
{
    test_name = "test_print_integer_list";
    char* test_string = "(240 -44 902)";
    lisp_interpreter_t interp;
    init_interpreter(&interp, 256);
    lisp_object_t obj = parse1(&interp, &test_string);
    char* result = print_object(obj);
    check(strcmp("(240 -44 902)", result) == 0, "string value");
    free(result);
}

static void test_print_dotted_pair()
{
    test_name = "test_print_dotted_pair";
    char* test_string = "(65 . 185)";
    lisp_interpreter_t interp;
    init_interpreter(&interp, 256);
    lisp_object_t obj = parse1(&interp, &test_string);
    char* result = print_object(obj);
    check(strcmp("(65 . 185)", result) == 0, "string value");
    free(result);
}

static void test_print_complex_list()
{
    test_name = "test_print_complex_list";
    char* test_string = "(1 (2 3 4 (5 (6 7 8 (9 . 0)))))";
    lisp_interpreter_t interp;
    init_interpreter(&interp, 256);
    lisp_object_t obj = parse1(&interp, &test_string);
    char* result = print_object(obj);
    check(strcmp("(1 (2 3 4 (5 (6 7 8 (9 . 0)))))", result) == 0, "string value");
    free(result);
}

static void test_nil_is_not_a_cons()
{
    test_name = "test_nil_is_not_a_cons";
    check(consp(NIL) == NIL, "not a cons");
}

static void test_t_is_not_a_cons()
{
    test_name = "test_t_is_not_a_cons";
    check(consp(T) == NIL, "not a cons");
}

static void test_nil_is_a_symbol()
{
    test_name = "test_nil_is_a_symbol";
    check(symbolp(NIL) == T, "symbol");
}

static void test_t_is_a_symbol()
{
    test_name = "test_t_is_a_symbol";
    check(symbolp(T) == T, "symbol");
}

static void test_read_and_print_nil()
{
    test_name = "read_and_print_nil";
    char* test_string = "nil";
    lisp_interpreter_t interp;
    init_interpreter(&interp, 256);
    lisp_object_t obj = parse1(&interp, &test_string);
    check(obj == NIL, "is nil");
    char* result = print_object(obj);
    check(strcmp("nil", result) == 0, "print nil");
    free(result);
}

static void test_read_and_print_t()
{
    test_name = "read_and_print_t";
    char* test_string = "t";
    lisp_interpreter_t interp;
    init_interpreter(&interp, 256);
    lisp_object_t obj = parse1(&interp, &test_string);
    check(obj == T, "is t");
    char* result = print_object(obj);
    check(strcmp("t", result) == 0, "print t");
    free(result);
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
    test_parse_symbol();
    return 0;
}
