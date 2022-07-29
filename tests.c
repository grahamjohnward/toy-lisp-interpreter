#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lisp.h>
#include <string_buffer.h>

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

static void test_parse_multiple_objects()
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
    test_t_is_not_a_cons();
    test_nil_is_a_symbol();
    test_t_is_a_symbol();
    test_read_and_print_nil();
    test_read_and_print_t();
    test_strings();
    test_print_empty_cons();
    test_symbol_pointer();
    test_parse_symbol();
    test_parse_multiple_symbols();
    test_parse_list_of_symbols();
    test_parse_string();
    test_parse_string_with_escape_characters();
    test_eq();
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
