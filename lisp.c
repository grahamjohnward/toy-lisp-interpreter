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

#define ConsPtr(obj) ((struct cons*)((obj)&PTR_MASK))
#define SymbolPtr(obj) ((struct symbol*)((obj)&PTR_MASK))
/* StringPtr is different as a string is not a struct */
#define StringPtr(obj) ((size_t*)((obj)&PTR_MASK))

/* Might be nice to have a lisp_memory struct separate from the interpreter */
struct lisp_interpreter {
    lisp_object_t* heap;
    lisp_object_t* next_free;
    lisp_object_t symbol_table; /* How will GC work for this? */
};

struct cons {
    lisp_object_t car;
    lisp_object_t cdr;
};

struct symbol {
    lisp_object_t name;
    lisp_object_t value;
    lisp_object_t function;
};

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

static void check_string(lisp_object_t o)
{
    if ((o & TYPE_MASK) != STRING_TYPE) {
        char* obj_string = print_object(o);
        printf("Not a string: %s\n", obj_string);
        free(obj_string);
        exit(1);
    }
}

static void check_symbol(lisp_object_t o)
{
    if ((o & TYPE_MASK) != SYMBOL_TYPE) {
        char* obj_string = print_object(o);
        printf("Not a symbol: %s\n", obj_string);
        free(obj_string);
        exit(1);
    }
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

lisp_object_t stringp(lisp_object_t obj)
{
    return (obj & TYPE_MASK) == STRING_TYPE ? T : NIL;
}

lisp_object_t symbolp(lisp_object_t obj)
{
    return (obj & TYPE_MASK) == SYMBOL_TYPE ? T : NIL;
}

lisp_object_t integerp(lisp_object_t obj)
{
    return (obj & TYPE_MASK) == INTEGER_TYPE ? T : NIL;
}

lisp_object_t consp(lisp_object_t obj)
{
    return ((obj & TYPE_MASK) == CONS_TYPE) ? T : NIL;
}

lisp_object_t allocate_cons(struct lisp_interpreter* interp)
{
    lisp_object_t new_cons = (lisp_object_t)interp->next_free;
    interp->next_free += 2;
    new_cons |= CONS_TYPE;
    rplaca(new_cons, NIL);
    rplacd(new_cons, NIL);
    return new_cons;
}

static void init_interpreter(struct lisp_interpreter* interp, size_t heap_size)
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
    interp->symbol_table = NIL;
}

lisp_object_t allocate_single_object(struct lisp_interpreter* interp)
{
    return (lisp_object_t)interp->next_free++;
}

lisp_object_t allocate_string(struct lisp_interpreter* interp, size_t len, char* str)
{
    lisp_object_t obj = (lisp_object_t)interp->next_free;
    size_t* header_address = (size_t*)obj;
    *header_address = len;
    char* straddr = (char*)(header_address + 1);
    strncpy(straddr, str, len);
    interp->next_free = (lisp_object_t*)(straddr + len + 8 - len % sizeof(lisp_object_t));
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
    if (eq(s1, s2) == T) {
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
    if (first == NIL)
        /* This should not happen - remove this case */
        return NIL;
    else if (string_equalp(symbol_name(first), name))
        return first;
    else
        return find_symbol(cdr(list_of_symbols), name);
}

lisp_object_t allocate_symbol(struct lisp_interpreter* interp, lisp_object_t name)
{
    lisp_object_t preexisting_symbol = find_symbol(interp->symbol_table, name);
    if (preexisting_symbol != NIL) {
        return preexisting_symbol;
    } else {
        check_string(name);
        lisp_object_t obj = (lisp_object_t)interp->next_free;
        struct symbol* s = (struct symbol*)obj;
        s->name = name;
        s->value = NIL;
        s->function = NIL;
        interp->next_free += 3;
        lisp_object_t symbol = obj | SYMBOL_TYPE;
        lisp_object_t new_cons = allocate_cons(interp);
        rplaca(new_cons, symbol);
        rplacd(new_cons, interp->symbol_table);
        interp->symbol_table = new_cons;
        return symbol;
    }
}

lisp_object_t parse_symbol(struct lisp_interpreter* interp, char** text)
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
        lisp_object_t lisp_string = allocate_string(interp, len, tmp);
        return allocate_symbol(interp, lisp_string);
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

lisp_object_t parse1(struct lisp_interpreter*, char**);

lisp_object_t parse_cons(struct lisp_interpreter* interp, char** text)
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

lisp_object_t parse1(struct lisp_interpreter* interp, char** text)
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

void parse(struct lisp_interpreter* interp, char* text,
    void (*callback)(lisp_object_t))
{
    char** cursor = &text;
    while (*text)
        callback(parse1(interp, cursor));
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
    link->string = malloc(link->len);
    strcpy(link->string, string);
}

void string_buffer_print(struct string_buffer_link* sb)
{
    for (struct string_buffer_link* link = sb; link; link = link->next)
        printf("%s", link->string);
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
    char* result = malloc(sb->len);
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
        char* tmp = malloc(len + 1);
        strncpy(tmp, strptr, len);
        tmp[len] = 0;
        string_buffer_append(sb, tmp);
        free(tmp);
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
    struct lisp_interpreter interp;
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
    struct lisp_interpreter interp;
    init_interpreter(&interp, 256);
    lisp_object_t result = parse1(&interp, &test_string);
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
}

static void test_parse_dotted_pair_of_integers()
{
    test_name = "parse_dotted_pair_of_integers";
    char* test_string = "(45 . 123)";
    struct lisp_interpreter interp;
    init_interpreter(&interp, 256);
    lisp_object_t result = parse1(&interp, &test_string);
    check(consp(result), "consp");
    check(integerp(car(result)), "car is int");
    check(integerp(cdr(result)), "cdr is int");
    check(car(result) == 45 << 3, "car value");
    check(cdr(result) == 123 << 3, "cdr value");
}

static void test_string_buffer()
{
    test_name = "test_string_buffer";
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
    test_name = "test_print_integer";
    char* test_string = "93";
    struct lisp_interpreter interp;
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
    struct lisp_interpreter interp;
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
    struct lisp_interpreter interp;
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
    struct lisp_interpreter interp;
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
    struct lisp_interpreter interp;
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
    struct lisp_interpreter interp;
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
    struct lisp_interpreter interp;
    init_interpreter(&interp, 256);
    lisp_object_t obj = parse1(&interp, &test_string);
    check(obj == T, "is t");
    char* result = print_object(obj);
    check(strcmp("t", result) == 0, "print t");
    free(result);
}

static void test_strings()
{
    test_name = "test_strings";
    struct lisp_interpreter interp;
    init_interpreter(&interp, 256);
    lisp_object_t s1 = allocate_string(&interp, 5, "hello");
    lisp_object_t s2 = allocate_string(&interp, 5, "hello");
    lisp_object_t s3 = allocate_string(&interp, 6, "oohaah");
    check(string_equalp(s1, s2) == T, "equal strings are equalp/1");
    check(string_equalp(s2, s1) == T, "equal strings are equalp/2");
    check(string_equalp(s1, s3) == NIL, "unequal strings are not equalp/1");
    check(string_equalp(s2, s3) == NIL, "unequal strings are not equalp/2");
    size_t len;
    char* str;
    get_string_parts(s1, &len, &str);
    check(len == 5, "get_string_parts/length");
    check(strncmp("hello", str, 5) == 0, "get_string_parts/string");
}

static void test_print_empty_cons()
{
    test_name = "test_print_empty_cons";
    struct lisp_interpreter interp;
    init_interpreter(&interp, 256);
    lisp_object_t empty = allocate_cons(&interp);
    check(strcmp("(nil)", print_object(empty)) == 0, "(nil)");
}

static void test_symbol_pointer()
{
    test_name = "test_symbol_pointer";
    lisp_object_t obj_without_tag = 8;
    lisp_object_t tagged_obj = obj_without_tag | SYMBOL_TYPE;
    struct symbol* ptr = SymbolPtr(tagged_obj);
    check((int)obj_without_tag == (int)ptr, "correct pointer");
}

static void test_parse_symbol()
{
    test_name = "test_parse_symbol";
    char* test_string = "foo";
    struct lisp_interpreter interp;
    init_interpreter(&interp, 256);
    lisp_object_t result = parse1(&interp, &test_string);
    check(symbolp(result) == T, "symbolp");
    check(consp(result) == NIL, "not consp");
    check(strcmp("foo", print_object(result)) == 0, "print");
}

static void test_parse_multiple_symbols()
{
    test_name = "test_parse_symbol";
    char* s1 = "foo";
    struct lisp_interpreter interp;
    init_interpreter(&interp, 256);
    lisp_object_t sym1 = parse1(&interp, &s1);
    char* s2 = "bar";
    lisp_object_t sym2 = parse1(&interp, &s2);
    check(strcmp("(bar foo)", print_object(interp.symbol_table)), "symbol table looks right");
    char* s3 = "bar";
    lisp_object_t sym3 = parse1(&interp, &s2);
    check(strcmp("(bar foo)", print_object(interp.symbol_table)), "symbol reused");
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
    test_parse_symbol();
    test_parse_multiple_symbols();
    return 0;
}
