#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "interp.h"
#include "lexical_scope.h"
#include "lisp.h"
#include "string_buffer.h"
#include "text_stream.h"
#include "vm.h"

static char *test_name; /* Global */

static int fail_count = 0;

static void check(int boolean, char *tag)
{
    printf("%s - %s", test_name, tag);
    printf("\t");
    if (boolean) {
        printf("ok");
    } else {
        fail_count++;
        printf("NOT OK");
    }
    printf("\n");
}

static lisp_object_t parse1_wrapper(char *str)
{
    struct text_stream ts;
    text_stream_init_str(&ts, str);
    return parse1(&ts);
}

static lisp_object_t parse_string_wrapper(char *str)
{
    struct text_stream ts;
    text_stream_init_str(&ts, str);
    return parse_string(&ts);
}

static void parse_wrapper(char *str, void (*callback)(void *, lisp_object_t), void *callback_data)
{
    struct text_stream ts;
    text_stream_init_str(&ts, str);
    parse(&ts, callback, callback_data);
}

static void init_interpreter_for_tests()
{
    init_interpreter(65536);
}

static int should_do_test(char *name)
{
    char *tests_to_run = getenv("TESTS_TO_RUN");
    if (!tests_to_run)
        return 1;
    char *tok = strtok(tests_to_run, ",");
    while (tok) {
        if (strcmp(name, tok) == 0)
            return 1;
        tok = strtok(NULL, ",");
    }
    return 0;
}

#define START_OF_TEST(name)       \
    do {                          \
        if (should_do_test(name)) \
            test_name = name;     \
        else                      \
            return;               \
    } while (0)

static void test_skip_whitespace()
{
    START_OF_TEST("skip_whitespace");
    char *test_string = "  hello";
    struct text_stream ts;
    text_stream_init_str(&ts, test_string);
    skip_whitespace(&ts);
    check(text_stream_peek(&ts) == 'h', "next char");
}

static void test_comments()
{
    START_OF_TEST("comments");
    char *test_string = "; This is a comment";
    struct text_stream ts;
    text_stream_init_str(&ts, test_string);
    skip_whitespace(&ts);
    check(text_stream_eof(&ts), "eof");
}

static void test_parse_integer()
{
    START_OF_TEST("parse_integer");
    char *test_string = "13";
    uint64_t result = parse1_wrapper(test_string);
    check(result == LispInt(13), "value");
}

static void test_parse_large_integer()
{
    START_OF_TEST("parse_large_integer");
    char *test_string = "1152921504606846975";
    uint64_t result = parse1_wrapper(test_string);
    check(result == LispInt(1152921504606846975), "value");
    check(integerp((lisp_object_t)result) != NIL, "integerp");
}

static void test_parse_negative_integer()
{
    START_OF_TEST("parse_negative_integer");
    char *test_string = "-498";
    uint64_t result = parse1_wrapper(test_string);
    check(result == -498 * LispInt(1), "value");
}

static void test_parse_large_negative_integer()
{
    START_OF_TEST("parse_large_negative_integer");
    char *test_string = "-1152921504606846976";
    uint64_t result = parse1_wrapper(test_string);
    check(result == ((uint64_t)-1152921504606846976 * LispInt(1)), "value");
    check(integerp((lisp_object_t)result) != NIL, "integerp");
}

static void test_integer_too_large()
{
    START_OF_TEST("test_integer_too_large");
    char *test_string = "1152921504606846976";
    /* Calls abort() */
    // int64_t result = parse_integer_wrapper(&test_string);
}

static void test_integer_too_negative()
{
    START_OF_TEST("test_integer_too_negative");
    char *test_string = "-1152921504606846977";
    /* Calls abort() */
    // int64_t result = parse_integer_wrapper(&test_string);
}

static void test_parse_single_integer_list()
{
    START_OF_TEST("parse_single_integer_list");
    lisp_object_t result = NIL;
    lisp_object_t result_car = NIL;
    lisp_object_t result_cdr = NIL;
    char *test_string = "(14)";
    init_interpreter_for_tests();
    result = parse1_wrapper(test_string);
    check(consp(result), "consp");
    result_car = car(result);
    check(integerp(result_car), "car is int");
    check(result_car == LispInt(14), "car value");
    result_cdr = cdr(result);
    check(NIL == result_cdr, "cdr is null");
    free_interpreter();
}

static void test_parse_integer_list()
{
    START_OF_TEST("parse_integer_list");
    char *test_string = "(23 71)";
    lisp_object_t result = NIL;
    lisp_object_t result_car = NIL;
    lisp_object_t result_cdr = NIL;
    lisp_object_t cadr = NIL;
    init_interpreter_for_tests();
    result = parse1_wrapper(test_string);
    check(consp(result), "consp");
    result_car = car(result);
    check(integerp(result_car), "car is int");
    check(result_car == LispInt(23), "car value");
    result_cdr = cdr(result);
    check(NIL != result_cdr, "cdr is not null");
    check(consp(result_cdr), "cdr is a pair");
    cadr = car(result_cdr);
    check(integerp(cadr), "cadr is int");
    check(cadr == LispInt(71), "cadr value");
    free_interpreter();
}

static void test_parse_dotted_pair_of_integers()
{
    START_OF_TEST("parse_dotted_pair_of_integers");
    char *test_string = "(45 . 123)";
    lisp_object_t result = NIL;
    init_interpreter_for_tests();
    result = parse1_wrapper(test_string);
    check(consp(result), "consp");
    check(integerp(car(result)), "car is int");
    check(integerp(cdr(result)), "cdr is int");
    check(car(result) == LispInt(45), "car value");
    check(cdr(result) == LispInt(123), "cdr value");
    free_interpreter();
}

static void test_string_buffer()
{
    START_OF_TEST("string_buffer");
    struct string_buffer sb;
    string_buffer_init(&sb);
    string_buffer_append(&sb, "foo");
    string_buffer_append(&sb, "bar");
    char *string = string_buffer_to_string(&sb);
    check(strcmp("foobar", string) == 0, "string value");
    check(sb.len == 6, "length");
    free(string);
    string_buffer_free_links(&sb);
}

static void test_print_integer()
{
    START_OF_TEST("print_integer");
    char *test_string = "93";
    lisp_object_t obj = NIL;
    init_interpreter_for_tests();
    obj = parse1_wrapper(test_string);
    char *result = print_object(obj);
    check(strcmp("93", result) == 0, "string value");
    free(result);
    free_interpreter();
}

static void test_print_single_integer_list()
{
    START_OF_TEST("print_single_integer_list");
    char *test_string = "(453)";
    lisp_object_t obj = NIL;
    init_interpreter_for_tests();
    obj = parse1_wrapper(test_string);
    char *result = print_object(obj);
    check(strcmp("(453)", result) == 0, "string value");
    free(result);
    free_interpreter();
}

static void test_print_integer_list()
{
    START_OF_TEST("print_integer_list");
    char *test_string = "(240 -44 902)";
    lisp_object_t obj = NIL;
    init_interpreter_for_tests();
    obj = parse1_wrapper(test_string);
    char *result = print_object(obj);
    check(strcmp("(240 -44 902)", result) == 0, "string value");
    free(result);
    free_interpreter();
}

static void test_print_dotted_pair()
{
    START_OF_TEST("print_dotted_pair");
    char *test_string = "(65 . 185)";
    init_interpreter_for_tests();
    lisp_object_t obj = parse1_wrapper(test_string);
    char *result = print_object(obj);
    check(strcmp("(65 . 185)", result) == 0, "string value");
    free(result);
    free_interpreter();
}

static void test_print_complex_list()
{
    START_OF_TEST("print_complex_list");
    char *test_string = "(1 (2 3 4 (5 (6 7 8 (9 . 0)))))";
    init_interpreter_for_tests();
    lisp_object_t obj = parse1_wrapper(test_string);
    char *result = print_object(obj);
    check(strcmp("(1 (2 3 4 (5 (6 7 8 (9 . 0)))))", result) == 0, "string value");
    free(result);
    free_interpreter();
}

static void test_nil_is_not_a_cons()
{
    START_OF_TEST("nil_is_not_a_cons");
    check(consp(NIL) == NIL, "not a cons");
}

static void test_t_is_not_a_cons()
{
    START_OF_TEST("t_is_not_a_cons");
    check(consp(T) == NIL, "not a cons");
}

static void test_nil_is_a_symbol()
{
    START_OF_TEST("nil_is_a_symbol");
    check(symbolp(NIL) == T, "symbol");
}

static void test_t_is_a_symbol()
{
    START_OF_TEST("t_is_a_symbol");
    check(symbolp(T) == T, "symbol");
}

static void test_read_and_print_nil()
{
    START_OF_TEST("read_and_print_nil");
    char *test_string = "nil";
    init_interpreter_for_tests();
    lisp_object_t obj = parse1_wrapper(test_string);
    check(obj == NIL, "is nil");
    char *result = print_object(obj);
    check(strcmp("nil", result) == 0, "print nil");
    free(result);
    free_interpreter();
}

static void test_read_and_print_t()
{
    START_OF_TEST("read_and_print_t");
    char *test_string = "t";
    init_interpreter_for_tests();
    lisp_object_t obj = parse1_wrapper(test_string);
    check(obj == T, "is T");
    char *result = print_object(obj);
    check(strcmp("t", result) == 0, "print t");
    free(result);
    free_interpreter();
}

static void test_read_empty_list()
{
    START_OF_TEST("read_empty_list");
    init_interpreter_for_tests();
    char *test_string = "()";
    lisp_object_t result = parse1_wrapper(test_string);
    check(result == NIL, "is nil");
    free_interpreter();
}

static void test_read_empty_list_in_list()
{
    START_OF_TEST("read_empty_list_in_list");
    lisp_object_t result = NIL;
    init_interpreter_for_tests();
    char *test_string = "(abc () xyz)";
    result = parse1_wrapper(test_string);
    char *str = print_object(result);
    check(strcmp(str, "(abc nil xyz)") == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_strings()
{
    START_OF_TEST("strings");
    lisp_object_t s1 = NIL;
    lisp_object_t s2 = NIL;
    lisp_object_t s3 = NIL;
    init_interpreter_for_tests();
    s1 = allocate_string(6, "hello");
    s2 = allocate_string(6, "hello");
    s3 = allocate_string(7, "oohaah");
    check(string_equalp(s1, s2) == T, "equal strings are equalp/1");
    check(string_equalp(s2, s1) == T, "equal strings are equalp/2");
    check(string_equalp(s1, s3) == NIL, "unequal strings are not equalp/1");
    check(string_equalp(s2, s3) == NIL, "unequal strings are not equalp/2");
    size_t len;
    char *str;
    get_string_parts(s1, &len, &str);
    check(len == 5, "get_string_parts/length");
    check(strncmp("hello", str, 5) == 0, "get_string_parts/string");
    free_interpreter();
}

static void test_print_empty_cons()
{
    START_OF_TEST("print_empty_cons");
    lisp_object_t empty = NIL;
    char *str = NULL;
    init_interpreter_for_tests();
    empty = cons(NIL, NIL);
    str = print_object(empty);
    check(strcmp("(nil)", str) == 0, "(nil)");
    free(str);
    free_interpreter();
}

static void test_symbol_pointer()
{
    START_OF_TEST("symbol_pointer");
    lisp_object_t obj_without_tag = 0x123400;
    lisp_object_t tagged_obj = obj_without_tag | SYMBOL_TYPE;
    struct symbol *ptr = SymbolPtr(tagged_obj);
    check((unsigned long)obj_without_tag == (unsigned long)ptr, "correct pointer");
}

static void test_parse_symbol()
{
    START_OF_TEST("parse_symbol");
    char *test_string = "foo";
    init_interpreter_for_tests();
    lisp_object_t result = parse1_wrapper(test_string);
    check(symbolp(result) == T, "symbolp");
    check(consp(result) == NIL, "not consp");
    char *str = print_object(result);
    check(strcmp("foo", str) == 0, "print");
    free(str);
    free_interpreter();
}

static void test_parse_multiple_symbols()
{
    START_OF_TEST("parse_multiple_symbols");
    lisp_object_t sym2 = NIL;
    lisp_object_t sym3 = NIL;
    char *s1 = "foo";
    init_interpreter_for_tests();
    interp->symbol_table = NIL;
    parse1_wrapper(s1);
    char *s2 = "bar";
    sym2 = parse1_wrapper(s2);
    char *str = print_object(interp->symbol_table);
    check(strcmp("(bar foo)", str) == 0, "symbol table looks right");
    free(str);
    char *s3 = "bar";
    sym3 = parse1_wrapper(s3);
    check(eq(sym2, sym3) == T, "symbols eq");
    str = print_object(interp->symbol_table);
    check(strcmp("(bar foo)", str) == 0, "symbol table looks right(2)");
    free(str);
    free_interpreter();
}

static void test_parse_list_of_symbols()
{
    START_OF_TEST("parse_list_of_symbols");
    char *test_string = "(hello you are nice)";
    lisp_object_t result = NIL;
    init_interpreter_for_tests();
    result = parse1_wrapper(test_string);
    check(consp(result) != NIL, "consp");
    check(symbolp(car((result))) != NIL, "first symbolp");
    char *str = print_object(result);
    check(strcmp("(hello you are nice)", str) == 0, "prints ok");
    free(str);
    free_interpreter();
}

static void test_parse_string()
{
    START_OF_TEST("parse_string");
    lisp_object_t obj = NIL;
    char *string = "\"hello\"";
    char *str = NULL;
    char *str2 = NULL;
    init_interpreter_for_tests();
    obj = parse_string_wrapper(string);
    check(stringp(obj), "stringp");
    size_t len;
    get_string_parts(obj, &len, &str);
    check(len == 5, "length");
    check(strcmp("hello", str) == 0, "value");
    str2 = print_object(obj);
    check(strcmp("\"hello\"", str2) == 0, "print_object");
    free(str2);
    free_interpreter();
}

static void test_parse_string_with_escape_characters()
{
    START_OF_TEST("parse_string_with_escape_characters");
    lisp_object_t obj = NIL;
    init_interpreter_for_tests();
    char *string = "\"he\\\"llo\n\t\r\"";
    obj = parse_string_wrapper(string);
    check(stringp(obj), "stringp");
    size_t len;
    char *str;
    get_string_parts(obj, &len, &str);
    check(len == 9, "length");
    check(strcmp("he\"llo\n\t\r", str) == 0, "value");
    str = print_object(obj);
    check(strcmp("\"he\\\"llo\\n\\t\\r\"", str) == 0, "printed");
    free(str);
    free_interpreter();
}

static void test_parse_list_of_strings()
{
    START_OF_TEST("parse_list_of_strings");
    lisp_object_t obj = NIL;
    init_interpreter_for_tests();
    char *string = "(\"hello\" \"world\")";
    obj = parse1_wrapper(string);
    check(consp(obj), "list returned");
    lisp_object_t s1 = car(obj);
    check(stringp(s1), "first element is string");
    char *s1str = print_object(s1);
    check(strcmp("\"hello\"", s1str) == 0, "first element ok");
    lisp_object_t s2 = car(cdr(obj));
    char *s2str = print_object(s2);
    check(strcmp("\"world\"", s2str) == 0, "second element ok");
    free(s1str);
    free(s2str);
    check(stringp(s2), "second element is string");
    free_interpreter();
}

static void test_eq()
{
    START_OF_TEST("eq");
    check(eq(0, 0) == T, "(eq 0 0) is t");
    check(eq(1, 1) == T, "(eq 1 1) is t");
    check(eq(NIL, NIL) == T, "(eq nil nil) is t");
    check(eq(T, T) == T, "(eq t t) is t");
    check(eq(0, 0) != NIL, "(eq 0 0) is not nil");
    check(eq(1, 1) != NIL, "(eq 1 1) is not nil");
    check(eq(NIL, NIL) != NIL, "(eq nil nil) is not nil");
    check(eq(T, T) != NIL, "(eq t t) is not nil");
}

static void test_parse_multiple_objects_callback(void *data, lisp_object_t obj)
{
    print_object_to_buffer(obj, (struct string_buffer *)data);
}

static void test_parse_multiple_objects()
{
    START_OF_TEST("parse_multiple_objects");
    char *test_string = "foo bar";
    init_interpreter_for_tests();
    struct string_buffer sb;
    string_buffer_init(&sb);
    parse_wrapper(test_string, test_parse_multiple_objects_callback, (void *)&sb);
    char *str = string_buffer_to_string(&sb);
    string_buffer_free_links(&sb);
    check(strcmp("foobar", str) == 0, "parses both symbols");
    free(str);
    free_interpreter();
}

static void test_parse_handle_eof_callback(void *data, lisp_object_t obj)
{
    int *count = (int *)data;
    (*count)++;
}

static void test_parse_handle_eof()
{
    START_OF_TEST("parse_handle_eof");
    char *test_string = "foo bar\n";
    struct string_buffer sb;
    string_buffer_init(&sb);
    init_interpreter_for_tests();
    int count = 0;
    parse_wrapper(test_string, test_parse_handle_eof_callback, &count);
    check(count == 2, "two objects");
    free_interpreter();
}

static void test_parse_quote()
{
    START_OF_TEST("parse_quote");
    char *test_string = "'FOO";
    lisp_object_t result = NIL;
    char *str = NULL;
    struct string_buffer sb;
    string_buffer_init(&sb);
    init_interpreter_for_tests();
    result = parse1_wrapper(test_string); // IT"S THIS
    print_object_to_buffer(result, &sb);
    str = string_buffer_to_string(&sb); // not this
    string_buffer_free_links(&sb); // not htis
    check(strcmp("'FOO", str) == 0, "parse quote"); // not this
    free(str);
    free_interpreter();
}

static void test_vector_initialization()
{
    START_OF_TEST("vector_initialization");
    lisp_object_t v = NIL;
    /* Initialize unused stack space allocated due to 16-byte stack alignment: */
    lisp_object_t dummy = NIL;
    init_interpreter_for_tests();
    v = allocate_vector(LispInt(3));
    check(eq(svref(v, 0), NIL) != NIL, "first element nil");
    check(eq(svref(v, 1), NIL) != NIL, "second element nil");
    check(eq(svref(v, 2), NIL) != NIL, "third element nil");
    free_interpreter();
}

static void test_vector_svref()
{
    START_OF_TEST("vector_svref");
    init_interpreter_for_tests();
    char *symbol_text = "foo";
    lisp_object_t sym = NIL;
    lisp_object_t v = NIL;
    lisp_object_t list = NIL;
    sym = parse1_wrapper(symbol_text);
    v = allocate_vector(LispInt(3));
    char *list_text = "(a b c)";
    list = parse1_wrapper(list_text);
    svref_set(v, 0, 14);
    svref_set(v, LispInt(1), sym);
    svref_set(v, LispInt(2), list);
    check(eq(svref(v, 0), 14) != NIL, "first element");
    check(eq(svref(v, LispInt(1)), sym) != NIL, "second element");
    check(eq(svref(v, LispInt(2)), list) != NIL, "third element");
    free_interpreter();
}

static void test_parse_vector()
{
    START_OF_TEST("parse_vector");
    char *text = "#(a b c)";
    init_interpreter_for_tests();
    lisp_object_t result = NIL;
    lisp_object_t sym_a = NIL;
    lisp_object_t sym_b = NIL;
    lisp_object_t sym_c = NIL;
    result = parse1_wrapper(text);
    check(vectorp(result) == T, "vectorp");
    char *a_text = "a";
    sym_a = parse1_wrapper(a_text);
    check(eq(sym_a, svref(result, 0)) == T, "first element");
    char *b_text = "b";
    sym_b = parse1_wrapper(b_text);
    check(eq(sym_b, svref(result, LispInt(1))) == T, "second element");
    char *c_text = "c";
    sym_c = parse1_wrapper(c_text);
    check(eq(sym_c, svref(result, LispInt(2))) == T, "third element");
    free_interpreter();
}

static void test_print_vector()
{
    START_OF_TEST("print_vector");
    char *text = "#(a b c)";
    init_interpreter_for_tests();
    lisp_object_t result = NIL;
    result = parse1_wrapper(text);
    char *str = print_object(result);
    check(strcmp("#(a b c)", str) == 0, "correct string");
    free(str);
    free_interpreter();
}

static void test_car_of_nil()
{
    START_OF_TEST("car_of_nil");
    check(car(NIL) == NIL, "car of nil is nil");
}

static void test_cdr_of_nil()
{
    START_OF_TEST("cdr_of_nil");
    check(cdr(NIL) == NIL, "cdr of nil is nil");
}

static void test_parse_list_of_dotted_pairs()
{
    START_OF_TEST("parse_list_of_dotted_pairs");
    init_interpreter_for_tests();
    char *text1 = "((X . SHAKESPEARE) (Y . (THE TEMPEST)))";
    lisp_object_t obj = parse1_wrapper(text1);
    char *str = print_object(obj);
    check(strcmp("((X . SHAKESPEARE) (Y THE TEMPEST))", str) == 0, "");
    free(str);
    free_interpreter();
}

static void test_sublis()
{
    START_OF_TEST("test_sublis");
    init_interpreter_for_tests();
    char *text1 = "((X . SHAKESPEARE) (Y . (THE TEMPEST)))";
    char *text2 = "(X WROTE Y)";
    lisp_object_t obj1 = parse1_wrapper(text1);
    lisp_object_t obj2 = parse1_wrapper(text2);
    lisp_object_t result = sublis(obj1, obj2);
    char *str = print_object(result);
    check(strcmp("(SHAKESPEARE WROTE (THE TEMPEST))", str) == 0, "");
    free(str);
    free_interpreter();
}

static void test_null()
{
    START_OF_TEST("null");
    check(null(NIL) != NIL, "nil");
    check(null(T) == NIL, "t");
}

static void test_append()
{
    START_OF_TEST("append");
    init_interpreter_for_tests();
    char *text1 = "(A B)";
    char *text2 = "(C D E)";
    lisp_object_t obj1 = parse1_wrapper(text1);
    lisp_object_t obj2 = parse1_wrapper(text2);
    lisp_object_t result = append(obj1, obj2);
    char *str = print_object(result);
    check(strcmp("(A B C D E)", str) == 0, "");
    free(str);
    free_interpreter();
}

static void test_member()
{
    START_OF_TEST("member");
    init_interpreter_for_tests();
    char *text1 = "A";
    char *text2 = "X";
    char *text3 = "(A B C D)";
    lisp_object_t obj1 = parse1_wrapper(text1);
    lisp_object_t obj2 = parse1_wrapper(text2);
    lisp_object_t obj3 = parse1_wrapper(text3);
    check(member(obj1, obj3) != NIL, "A is member");
    check(member(obj2, obj3) == NIL, "X is not member");
    free_interpreter();
}

static void test_assoc()
{
    START_OF_TEST("assoc");
    init_interpreter_for_tests();
    lisp_object_t b = NIL;
    lisp_object_t x = NIL;
    lisp_object_t result = NIL;
    char *text1 = "((A . (M N)) (B . (car X)) (C . (quote M)) (C . (cdr x)))";
    char *text2 = "B";
    char *text3 = "X";
    lisp_object_t alist = parse1_wrapper(text1);
    b = parse1_wrapper(text2);
    x = parse1_wrapper(text3);
    result = assoc(b, alist);
    char *str = print_object(result);
    check(strcmp("(B car X)", str) == 0, "match found");
    free(str);
    check(assoc(x, alist) == NIL, "match not present");
    free_interpreter();
}

static void test_sym()
{
    START_OF_TEST("sym");
    lisp_object_t x1 = NIL;
    lisp_object_t x2 = NIL;
    lisp_object_t y = NIL;
    init_interpreter_for_tests();
    x1 = sym("x");
    x2 = sym("x");
    y = sym("y");
    check(eq(x1, x2) != NIL, "(eq x1 x2)");
    check(eq(x1, y) == NIL, "(not (eq x1 y))");
    check(eq(x2, y) == NIL, "(not (eq x2 y))");
    free_interpreter();
}

static void test_evalquote_helper(char *fnstr, char *exprstr, char *expected)
{
    init_interpreter_for_tests();
    char *fnstr_copy = fnstr;
    lisp_object_t fn = parse1_wrapper(fnstr);
    lisp_object_t expr = parse1_wrapper(exprstr);
    lisp_object_t result = evalquote(fn, expr);
    char *result_str = print_object(result);
    check(strcmp(expected, result_str) == 0, fnstr_copy);
    free(result_str);
    free_interpreter();
}

static void test_evalquote()
{
    START_OF_TEST("evalquote");
    test_evalquote_helper("car", "((A . B))", "A");
    test_evalquote_helper("cdr", "((A . B))", "B");
    test_evalquote_helper("cdr", "((A . B))", "B");
    test_evalquote_helper("atom", "(A)", "t");
    test_evalquote_helper("atom", "((A . B))", "nil");
    test_evalquote_helper("eq", "(A A)", "t");
    test_evalquote_helper("eq", "(A B)", "nil");
}

static lisp_object_t test_eval_string_helper(char *exprstr)
{
    lisp_object_t expr = NIL, result = NIL;
    expr = parse1_wrapper(exprstr);
    result = eval_toplevel(expr);
    return result;
}

static void test_eval_helper(char *exprstr, char *expectedstr)
{
    init_interpreter_for_tests();
    char *exprstr_save = exprstr;
    char *stuff = NULL;
    lisp_object_t result = test_eval_string_helper(exprstr);
    char *resultstr = print_object(result);
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
    stuff = string_buffer_to_string(&sb);
    check(ok, stuff);
    free(stuff);
    free(resultstr);
    string_buffer_free_links(&sb);
    free_interpreter();
}

static void test_eval()
{
    START_OF_TEST("eval");
    test_eval_helper("t", "t");
    test_eval_helper("3", "3");
    test_eval_helper("(cons (quote A) (quote B))", "(A . B)");
    test_eval_helper("(if (eq (car (cons (quote A) nil)) (quote A)) (quote OK))", "OK");
    test_eval_helper("(if (eq (car (cons (quote A) nil)) (quote B)) (quote BAD) (quote OK))", "OK");
    test_eval_helper("(funcall (function (lambda (X) (car X))) (cons (quote A) (quote B)))", "A");
}

static void test_load1()
{
    START_OF_TEST("load");
    lisp_object_t result1 = test_eval_string_helper("(load \"/home/graham/toy-lisp-interpreter/test-load.lisp\")");
    check(result1 == T, "load returns T");
    lisp_object_t result2 = test_eval_string_helper("(test1 (quote there))");
    char *str = print_object(result2);
    check(strcmp("(hello . there)", str) == 0, "result of test1");
    free(str);
}

static void test_load()
{
    init_interpreter_for_tests();
    test_load1();
    free_interpreter();
}

lisp_object_t test_fn(lisp_object_t a, lisp_object_t b)
{
    return cons(b, a);
}

static void test_set()
{
    START_OF_TEST("set");
    test_eval_helper("(let ((x 12)) (set 'x 14) x)", "14");
    test_eval_helper("(funcall (function (lambda (x) (set 'x 14) x)) 12)", "14");
}

static void test_rplaca()
{
    START_OF_TEST("rplaca");
    test_eval_helper("(let (x) (set 'x (cons 12 13)) (rplaca x 4) (car x))", "4");
}

static void test_rplacd()
{
    START_OF_TEST("rplacd");
    test_eval_helper("(let (x) (set 'x (cons 3 5)) (rplacd x 7) (cdr x))", "7");
}

static void test_rest_args()
{
    START_OF_TEST("rest_args");
    init_interpreter_for_tests();
    char *teststr = "(set-symbol-function 'foo #'(lambda (a b &rest c) (cons c (cons b a))))";
    eval_toplevel(parse1_wrapper(teststr));
    teststr = "(foo 1 2 3)";
    lisp_object_t result = eval_toplevel(parse1_wrapper(teststr));
    char *str = print_object(result);
    check(strcmp("(3 2 . 1)", str), "result");
    free(str);
    free_interpreter();
}

static void test_plus()
{
    START_OF_TEST("plus");
    test_eval_helper("(two-arg-plus 3 4)", "7");
}

static void test_minus()
{
    START_OF_TEST("minus");
    test_eval_helper("(two-arg-minus 7 4)", "3");
}

static void test_times()
{
    START_OF_TEST("times");
    test_eval_helper("(two-arg-times 3 4)", "12");
    test_eval_helper("(two-arg-times -3 4)", "-12");
    test_eval_helper("(two-arg-times 65536 65536)", "4294967296");
}

static void test_divide()
{
    START_OF_TEST("divide");
    test_eval_helper("(two-arg-divide 256 -2)", "-128");
}

static void test_read_token()
{
    START_OF_TEST("read_token");
    char *test_str = "abc d";
    struct text_stream ts;
    text_stream_init_str(&ts, test_str);
    char *result = read_token(&ts);
    check(strcmp("abc", result) == 0, "abc");
    check(text_stream_peek(&ts) == ' ', "stream advanced");
    free(result);
}

static void test_numeric_equals()
{
    START_OF_TEST("numeric_equals");
    test_eval_helper("(= 3 3)", "t");
    test_eval_helper("(= 4 3)", "nil");
}

static void test_parse_function_pointer()
{
    START_OF_TEST("parse_function_pointer");
    char *teststr = "0x2468";
    lisp_object_t result = parse1_wrapper(teststr);
    check(function_pointer_p(result) != NIL, "function_pointer_p");
    check(FunctionPtr(result) == (void (*)())0x2468, "value");
}

static void test_print_function_pointer()
{
    START_OF_TEST("print_function_pointer");
    lisp_object_t fp = FUNCTION_POINTER_TYPE | 0x2468;
    char *str = print_object(fp);
    check(strcmp("0x2468", str) == 0, "0x2468");
    free(str);
}

static void test_call_function_pointer()
{
    START_OF_TEST("call_function_pointer");
    /* This actually works if you can get the address right */
    // test_eval_helper("((built-in-function 0x404f90 2) 3 4)", "(3 . 4)");
}

static void test_integer_bug()
{
    START_OF_TEST("integer_bug");
    test_eval_helper("(two-arg-minus (two-arg-minus 123 12) 312312)", "-312201");
}

static void test_condition_case()
{
    START_OF_TEST("condition_case");
    test_eval_helper("(condition-case e (raise 'ohno 14) (ohno (cons 'error-was e)) (didnt-happen 'frob))", "(error-was ohno . 14)");
}

static void test_functionp()
{
    START_OF_TEST("functionp");
    init_interpreter_for_tests();
    check(functionp(test_eval_string_helper("(function (lambda (x) (cons x x)))")) == T, "lambda t");
    check(functionp(test_eval_string_helper("(function cons)")) == T, "cons t");
    check(functionp(parse1_wrapper("foo")) == NIL, "symbol nil");
    check(functionp(test_eval_string_helper("14")) == NIL, "integer nil");
    free_interpreter();
}

static void test_print_function()
{
    START_OF_TEST("print_function");
    test_eval_helper("(function (lambda (x) (cons x x)))", "#<function>");
    test_eval_helper("(function cons)", "#<function>");
}

static void test_unbound_variable()
{
    START_OF_TEST("unbound_variable");
    test_eval_helper("(condition-case e (print x) (unbound-variable (cons 'ohdear e)))", "(ohdear unbound-variable . x)");
}

static void test_plist()
{
    START_OF_TEST("plist");
    test_eval_helper("(progn (putprop 'foo 'greeting '(hello world)) (get 'foo 'greeting))", "(hello world)");
}

static void define_defmacro()
{
    lisp_object_t ignored = NIL;
    char *defmacro_str = "(progn"
                         "  (set-symbol-function 'defmacro"
                         "                      #'(lambda (name arglist &body body)"
                         "                          `(progn"
                         "                             (let ((result (set-symbol-function ',name #'(lambda ,arglist (block ,name ,@body)))))"
                         "                               (putprop ',name 'macro 't)"
                         "                               result))))"
                         "  (putprop 'defmacro 'macro 't))";
    ignored = test_eval_string_helper(defmacro_str);
}

static void test_defmacro()
{
    START_OF_TEST("defmacro");
    lisp_object_t result = NIL;
    init_interpreter_for_tests();
    define_defmacro();
    test_eval_string_helper("(defmacro if2 (test then else) `(if ,test ,then ,else))");
    result = test_eval_string_helper("(if2 (eq (car (cons 3 4)) 3) (two-arg-plus 9 9) 'bof)");
    check(result == LispInt(18), "test1");
    result = test_eval_string_helper("(if2 (eq (car (cons 3 4)) 4) (two-arg-plus 9 9) 'bof)");
    check(eq(result, sym("bof")) != NIL, "test2");
    free_interpreter();
}

static void test_unquote_splice()
{
    START_OF_TEST("unquote_splice");
    init_interpreter_for_tests();
    define_defmacro();
    test_eval_string_helper("(defmacro when (test &body then) `(if ,test (progn ,@then) nil))");
    lisp_object_t result = test_eval_string_helper("(let ((x 'foo)) (when (eq (car (cons 3 2)) 3) (set 'x 'bof) (cons x 14)))");
    char *str = print_object(result);
    check(strcmp("(bof . 14)", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_optional_arguments()
{
    START_OF_TEST("optional_arguments");
    init_interpreter_for_tests();
    eval_toplevel(parse1_wrapper("(set-symbol-function 'test #'(lambda (a &optional b) (cons 'hello (cons a (cons b 'foo)))))"));
    lisp_object_t result = test_eval_string_helper("(test 3 4)");
    char *str = print_object(result);
    check(strcmp(str, "(hello 3 4 . foo)") == 0, "provided");
    free(str);
    result = test_eval_string_helper("(test 3)");
    str = print_object(result);
    check(strcmp(str, "(hello 3 nil . foo)") == 0, "not provided");
    free(str);
    free_interpreter();
}

// use let instead
static void test_progn()
{
    START_OF_TEST("progn");
    init_interpreter_for_tests();
    lisp_object_t result = test_eval_string_helper("(let ((x 3) (y 4)) (progn (set 'x 12) (set 'y 13) (cons x y)))");
    char *str = print_object(result);
    check(strcmp(str, "(12 . 13)") == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_tagbody()
{
    START_OF_TEST("tagbody");
    init_interpreter_for_tests();
    lisp_object_t result = test_eval_string_helper("(let ((x 10)) (block b (tagbody iterate (if (= x 0) (return-from b 'done) (progn (set 'x (two-arg-minus x 1)) (go iterate))))))");
    char *str = print_object(result);
    check(strcmp("done", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_tagbody_bug()
{
    START_OF_TEST("tagbody_bug");
    init_interpreter_for_tests();
    lisp_object_t result = test_eval_string_helper("(let ((x 2)) (progn (tagbody (set 'x 14)) x))");
    check(result == LispInt(14), "ok");
    free_interpreter();
}

static void test_tagbody_returns_nil()
{
    START_OF_TEST("tagbody_returns_nil");
    test_eval_helper("(tagbody 14)", "nil");
}

// not sure about this one
static void test_tagbody_condition_case()
{
    START_OF_TEST("tagbody_condition_case");
    init_interpreter_for_tests();
    lisp_object_t result = test_eval_string_helper("(block b (tagbody (condition-case e (raise 'ohno) (ohno (go hello))) (return-from b 'bad) hello (return-from b 'hello)))");
    char *str = print_object(result);
    check(strcmp("hello", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_let()
{
    START_OF_TEST("let");
    test_eval_helper("(let ((a 3) (b (two-arg-plus 10 2)) (c 'frob) (d 14) x) (set 'd 8) (cons (two-arg-plus a b) (cons c (cons x d))))", "(15 frob nil . 8)");
}

static void test_macroexpand1()
{
    START_OF_TEST("macroexpand1");
    lisp_object_t expr = NIL;
    lisp_object_t result = NIL;
    char *str = NULL;
    init_interpreter_for_tests();
    define_defmacro();
    test_eval_string_helper("(defmacro ooh (x) `(aah ,x))");
    test_eval_string_helper("(defmacro aah (x) `(bar ,x))");
    expr = parse1_wrapper("(ooh (frob))");
    result = macroexpand1(expr, NIL);
    str = print_object(result);
    check(strcmp("((aah (frob)) . t)", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_macroexpand()
{
    START_OF_TEST("macroexpand");
    lisp_object_t expr = NIL;
    lisp_object_t result = NIL;
    char *str = NULL;
    init_interpreter_for_tests();
    define_defmacro();
    test_eval_string_helper("(defmacro ooh (x) `(aah ,x))");
    test_eval_string_helper("(defmacro aah (x) `(bar ,x))");
    expr = parse1_wrapper("(ooh (frob))");
    result = macroexpand(expr, NIL);
    str = print_object(result);
    check(strcmp("(bar (frob))", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_macro_inside_quasiquote()
{
    START_OF_TEST("macro_inside_quasiquote");
    lisp_object_t result = NIL;
    lisp_object_t parsed = NIL;
    char *str = NULL;
    init_interpreter(65536);
    define_defmacro();
    test_eval_string_helper("(defmacro prog1- (&body forms)"
                            "  (let ((first (car forms))"
                            "        (result (gensym)))"
                            "    `(progn"
                            "       (let ((,result ,first))"
                            "           ,@(cdr forms)"
                            "           ,result))))");
    parsed = parse1_wrapper("(,(prog1- (cons 1 2) 'foo) ,(prog1- (cons 3 4) 'bar) #(,(prog1- 'a 'b) 14 15))");
    result = macroexpand_all_quasiquote(parsed, 1);
    str = print_object(result);
    check(strcmp("(,(progn (let ((g0 (cons 1 2))) 'foo g0)) ,(progn (let ((g1 (cons 3 4))) 'bar g1)) #(,(progn (let ((g2 'a)) 'b g2)) 14 15))", str) == 0, "ok");
    free_interpreter();
}

static void test_macroexpand_all_if()
{
    START_OF_TEST("macroexpand_all_if");
    init_interpreter_for_tests();
    define_defmacro();
    test_eval_string_helper("(defmacro ooh (x) `(aah ,x))");
    test_eval_string_helper("(defmacro aah (x) `(bar ,x))");
    lisp_object_t expr = parse1_wrapper("(if nil 'ooh (ooh (frob)))");
    lisp_object_t result = macroexpand_all(expr);
    char *str = print_object(result);
    check(strcmp("(if nil 'ooh (bar (frob)))", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_macroexpand_all_progn()
{
    START_OF_TEST("macroexpand_all_progn");
    init_interpreter_for_tests();
    define_defmacro();
    lisp_object_t expr = NIL;
    lisp_object_t result = NIL;
    test_eval_string_helper("(defmacro ooh (x) `(aah ,x))");
    test_eval_string_helper("(defmacro aah (x) `(bar ,x))");
    expr = parse1_wrapper("(progn (ooh (frob)) (aah (hello)))");
    result = macroexpand_all(expr);
    char *str = print_object(result);
    check(strcmp("(progn (bar (frob)) (bar (hello)))", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_macroexpand_all_lambda()
{
    START_OF_TEST("macroexpand_all_lambda");
    init_interpreter_for_tests();
    define_defmacro();
    lisp_object_t expr = NIL;
    lisp_object_t result = NIL;
    test_eval_string_helper("(defmacro ooh (x) `(aah ,x))");
    test_eval_string_helper("(defmacro aah (x) `(bar ,x))");
    expr = parse1_wrapper("(lambda (x) (ooh (frob)) (aah (hello)))");
    result = macroexpand_all(expr);
    char *str = print_object(result);
    check(strcmp("(lambda (x) (bar (frob)) (bar (hello)))", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_macroexpand_all_tagbody()
{
    START_OF_TEST("macroexpand_all_tagbody");
    init_interpreter_for_tests();
    define_defmacro();
    lisp_object_t expr = NIL;
    lisp_object_t result = NIL;
    test_eval_string_helper("(defmacro ooh (x) `(aah ,x))");
    test_eval_string_helper("(defmacro aah (x) `(bar ,x))");
    expr = parse1_wrapper("(tagbody (ooh (frob)) foo (aah (hello)) (go foo))");
    result = macroexpand_all(expr);
    char *str = print_object(result);
    check(strcmp("(tagbody (bar (frob)) foo (bar (hello)) (go foo))", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_macroexpand_all_prog()
{
    START_OF_TEST("macroexpand_all_prog");
    init_interpreter_for_tests();
    define_defmacro();
    lisp_object_t expr = NIL;
    lisp_object_t result = NIL;
    test_eval_string_helper("(defmacro ooh (x) `(aah ,x))");
    test_eval_string_helper("(defmacro aah (x) `(bar ,x))");
    expr = parse1_wrapper("(prog (a b) (ooh (frob)) foo (aah (hello)) (go foo))");
    result = macroexpand_all(expr);
    char *str = print_object(result);
    check(strcmp("(prog (a b) (bar (frob)) foo (bar (hello)) (go foo))", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_macroexpand_all_set()
{
    START_OF_TEST("macroexpand_all_set");
    init_interpreter_for_tests();
    define_defmacro();
    lisp_object_t expr = NIL;
    lisp_object_t result = NIL;
    test_eval_string_helper("(defmacro frob (x) 'x)");
    test_eval_string_helper("(defmacro aah (x) `(bar ,x))");
    expr = parse1_wrapper("(set (frob 3) (aah (hello)))");
    result = macroexpand_all(expr);
    char *str = print_object(result);
    check(strcmp("(set x (bar (hello)))", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_macroexpand_all_let()
{
    START_OF_TEST("macroexpand_all_let");
    init_interpreter_for_tests();
    define_defmacro();
    lisp_object_t expr = NIL;
    lisp_object_t result = NIL;
    test_eval_string_helper("(defmacro ooh (x) `(aah ,x))");
    test_eval_string_helper("(defmacro aah (x) `(bar ,x))");
    expr = parse1_wrapper("(let ((a 14) (b (ooh y))) (ooh b))");
    result = macroexpand_all(expr);
    char *str = print_object(result);
    check(strcmp("(let ((a 14) (b (bar y))) (bar b))", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_macroexpand_all_defmacro()
{
    START_OF_TEST("macroexpand_all_defmacro");
    init_interpreter_for_tests();
    define_defmacro();
    lisp_object_t expr = NIL;
    lisp_object_t result = NIL;
    test_eval_string_helper("(defmacro ooh (x) `(aah ,x))");
    test_eval_string_helper("(defmacro aah (x) `(bar ,x))");
    expr = parse1_wrapper("(defmacro mymacro (a b) (ooh a) (aah b))");
    result = macroexpand_all(expr);
    char *str = print_object(result);
    check(strcmp("(progn (let ((result (set-symbol-function 'mymacro (function (lambda (a b) (block mymacro (bar a) (bar b))))))) (putprop 'mymacro 'macro 't) result))", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_macroexpand_all_condition_case()
{
    START_OF_TEST("macroexpand_all_condition_case");
    init_interpreter_for_tests();
    define_defmacro();
    lisp_object_t expr = NIL;
    lisp_object_t result = NIL;
    test_eval_string_helper("(defmacro ooh (x) `(aah ,x))");
    test_eval_string_helper("(defmacro aah (x) `(bar ,x))");
    expr = parse1_wrapper("(condition-case e (ooh 3) (ohno (aah 9)) (didnt-happen e))");
    result = macroexpand_all(expr);
    char *str = print_object(result);
    check(strcmp("(condition-case e (bar 3) (ohno (bar 9)) (didnt-happen e))", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_lambda_implicit_progn()
{
    START_OF_TEST("lambda_implicit_progn");
    test_eval_helper("(funcall (function (lambda (a b) (set 'a 12) (set 'b 14) (cons a b))) 3 4)", "(12 . 14)");
}

static void test_lisp_heap_cons()
{
    START_OF_TEST("lisp_heap_cons");
    init_interpreter_for_tests();
    struct lisp_heap *heap = &interp->heap;
    char *oldfreeptr = heap->freeptr;
    lisp_object_t new_cons = cons(NIL, T);
    struct cons *consptr = ConsPtr(new_cons);
    check(consptr->car == NIL, "car");
    check(consptr->cdr == T, "cdr");
    check(consptr->header == CONS_TYPE, "header");
    check(heap->freeptr - oldfreeptr == sizeof(struct cons), "freeptr");
    free_interpreter();
}

static void test_lisp_heap_copy_single_object()
{
    START_OF_TEST("lisp_heap_copy_single_object");
    struct lisp_heap heap;
    lisp_heap_init(&heap, 1024);
    struct cons *new_cons = (struct cons *)heap.freeptr;
    lisp_object_t new_cons_obj = (uint64_t)new_cons | CONS_TYPE;
    /* Start allocating in the to-space as if we are doing GC */
    heap.freeptr = heap.to_space;
    gc_copy(&heap, &new_cons_obj);
    lisp_heap_free(&heap);
}

static void test_lisp_heap_gc_simple()
{
    START_OF_TEST("lisp_heap_gc_simple");
    init_interpreter_for_tests();
    char *orig_from_space = interp->heap.from_space;
    char *orig_to_space = interp->heap.to_space;
    check(orig_from_space == interp->heap.heap, "from_space");
    check(orig_to_space == orig_from_space + interp->heap.size_bytes / 2, "to_space");
    free_interpreter();
}

static void test_vector_builtins()
{
    START_OF_TEST("vector_builtins");
    test_eval_helper("(let ((x (make-vector 4))) (set-svref x 3 'frob) (set-svref x 2 14) (cons x (cons (svref x 3) (cons (svref x 2)))))", "(#(nil nil 14 frob) frob 14)");
}

static void test_non_symbol_in_function_position()
{
    START_OF_TEST("non_symbol_in_function_position");
    test_eval_helper("(condition-case e (eval '(2 2)) (bad-expression e))", "(bad-expression 2 2)");
}

static void test_type_of()
{
    START_OF_TEST("type_of");
    test_eval_helper("(type-of 14)", "integer");
    test_eval_helper("(type-of 'foo)", "symbol");
    test_eval_helper("(type-of (cons 'a 'b))", "cons");
    test_eval_helper("(type-of \"hello\")", "string");
    test_eval_helper("(type-of #(1 2 3))", "vector");
}

static void test_comma_not_inside_backquote()
{
    START_OF_TEST("comma_not_inside_backquote");
    test_eval_helper("(condition-case e (eval ',foo) (runtime-error e))", "(runtime-error . comma-not-inside-backquote)");
}

static void test_string_equalp()
{
    START_OF_TEST("string_equalp");
    test_eval_helper("(string-equal-p \"foo\" \"foo\")", "t");
    test_eval_helper("(string-equal-p \"foo\" \"bar\")", "nil");
}

static void test_length_builtin()
{
    START_OF_TEST("length_builtin");
    test_eval_helper("(length '(a b c))", "3");
    test_eval_helper("(length #(1 2 3 4 5))", "5");
    test_eval_helper("(length #( ))", "0");
    test_eval_helper("(length nil)", "0");
}

static void test_parse_empty_vector()
{
    START_OF_TEST("parse_empty_vector");
    test_eval_helper("(type-of #())", "vector");
    test_eval_helper("(length #())", "0");
    test_eval_helper("#()", "#()");
}

static void test_quasiquote_bug()
{
    START_OF_TEST("quasiquote_bug");
    test_eval_helper("``(foo ,bar)", "`(foo ,bar)");
    test_eval_helper("(let ((bar 14)) ``(foo ,,bar))", "`(foo ,14)");
    test_eval_helper("``(foo ,@bar)", "`(foo ,@bar)");
}

static void test_apply()
{
    START_OF_TEST("apply");
    test_eval_helper("(apply 'cons '(a b))", "(a . b)");
}

static void test_parse_function()
{
    START_OF_TEST("parse_function");
    init_interpreter_for_tests();
    lisp_object_t result = parse1_wrapper("#'cons");
    char *str = print_object(result);
    check(strcmp("(function cons)", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_nonexistent_function()
{
    START_OF_TEST("nonexistent_function");
    init_interpreter_for_tests();
    lisp_object_t result = NIL;
    result = test_eval_string_helper("(condition-case e (function nonexistent) (undefined-function e))");
    char *str = print_object(result);
    check(strcmp("(undefined-function . nonexistent)", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_unquote_splice_bug()
{
    START_OF_TEST("unquote_splice_bug");
    init_interpreter_for_tests();
    lisp_object_t result = test_eval_string_helper("(let ((x '(1 2 3))) `(foo ,@x bar))");
    char *str = print_object(result);
    char *expected = "(foo 1 2 3 bar)";
    check(strcmp(expected, str) == 0, expected);
    free(str);
    free_interpreter();
}

static void test_gensym()
{
    START_OF_TEST("gensym");
    init_interpreter_for_tests();
    lisp_object_t result = gensym();
    char *str = print_object(result);
    check(strcmp("g0", str) == 0, "g0");
    check(symbolp(result) != NIL, "symbol");
    free(str);
    result = gensym();
    str = print_object(result);
    check(strcmp("g1", str) == 0, "g1");
    free(str);
    result = test_eval_string_helper("(gensym)");
    check(symbolp(result) != NIL, "built-in - symbol");
    str = print_object(result);
    check(strcmp("g2", str) == 0, "built-in - g2");
    free(str);
    free_interpreter();
}

static void test_varargs_list()
{
    START_OF_TEST("varargs_list");
    init_interpreter_for_tests();
    lisp_object_t mylist = list(interp->syms.lambda, sym("hello"), VARARGS_LIST_SENTINEL);
    char *str = print_object(mylist);
    check(strcmp("(lambda hello)", str) == 0, "function");
    free(str);
    mylist = List(interp->syms.lambda, LispInt(2));
    str = print_object(mylist);
    check(strcmp("(lambda 2)", str) == 0, "macro");
    free(str);
    free_interpreter();
}

static void test_block()
{
    START_OF_TEST("block");
    init_interpreter_for_tests();
    lisp_object_t result = test_eval_string_helper("(block foo (return-from foo 12) (print \"shouldn't happen\"))");
    char *str = print_object(result);
    check(strcmp("12", str) == 0, "ok");
    free_interpreter();
}
static void test_nil_block()
{
    START_OF_TEST("nil_block");
    init_interpreter_for_tests();
    lisp_object_t result = test_eval_string_helper("(block nil (return-from nil 12) (print \"shouldn't happen\"))");
    char *str = print_object(result);
    check(strcmp("12", str) == 0, "ok");
    free_interpreter();
}

static void test_nested_block()
{
    START_OF_TEST("nested_block");
    init_interpreter_for_tests();
    lisp_object_t result = test_eval_string_helper("(block foo (block bar (return-from foo 12) (print \"shouldn't happen\")) 14)");
    char *str = print_object(result);
    check(strcmp("12", str) == 0, "ok");
    free_interpreter();
}

static void test_nested_nil_block()
{
    START_OF_TEST("nested_nil_block");
    init_interpreter_for_tests();
    lisp_object_t result = test_eval_string_helper("(block nil (cons (block nil (return-from nil 12) (print \"shouldn't happen\")) 14))");
    char *str = print_object(result);
    check(strcmp("(12 . 14)", str) == 0, "ok");
    free_interpreter();
}

static void test_compile_bug()
{
    START_OF_TEST("compile_bug");
    test_eval_helper("(block foo (return-from foo (cons 12 (block bar (return-from bar 23)))))", "(12 . 23)");
}

static void test_macroexpand_function_lambda()
{
    START_OF_TEST("macroexpand_function_lambda");
    init_interpreter_for_tests();
    define_defmacro();
    test_eval_string_helper("(defmacro my-block (&body stuff) `(block nil ,@stuff))");
    lisp_object_t result = test_eval_string_helper("(funcall (function (lambda (x) (my-block (return-from nil (cons x 'hello))))) 12)");
    char *str = print_object(result);
    check(strcmp("(12 . hello)", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_set_symbol_function()
{
    START_OF_TEST("set_symbol_function");
    init_interpreter_for_tests();
    test_eval_string_helper("(set-symbol-function 'boo #'(lambda (x) (cons ':boo x)))");
    lisp_object_t result = test_eval_string_helper("(boo 14)");
    char *str = print_object(result);
    check(strcmp("(:boo . 14)", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_set_symbol_value()
{
    START_OF_TEST("set_symbol_value");
    init_interpreter_for_tests();
    lisp_object_t result = test_eval_string_helper("(progn (set-symbol-value 'foo 14) (symbol-value 'foo))");
    char *str = print_object(result);
    check(strcmp("14", str) == 0, "basic");
    result = test_eval_string_helper("(symbol-value 'bar)");
    str = print_object(result);
    check(strcmp("nil", str) == 0, "unset");
    free(str);
    free_interpreter();
}

static void test_compile_condition_case()
{
    START_OF_TEST("compile_condition_case");
    test_eval_helper("(condition-case e (let ((result 14)) (cons result result)))", "(14 . 14)");
}

static void test_if()
{
    START_OF_TEST("if");
    test_eval_helper("(if t 'a 'b)", "a");
    test_eval_helper("(if nil 'a 'b)", "b");
    test_eval_helper("(if t 'a)", "a");
    test_eval_helper("(if nil 'a)", "nil");
}

static void test_less_than()
{
    START_OF_TEST("less_than");
    test_eval_helper("(two-arg-less-than 3 2)", "nil");
    test_eval_helper("(two-arg-less-than 2 3)", "t");
    test_eval_helper("(two-arg-less-than -3 2)", "t");
    test_eval_helper("(two-arg-less-than 2 -3)", "nil");
}

static void test_greater_than()
{
    START_OF_TEST("greater_than");
    test_eval_helper("(two-arg-greater-than 3 2)", "t");
    test_eval_helper("(two-arg-greater-than 2 3)", "nil");
    test_eval_helper("(two-arg-greater-than -3 2)", "nil");
    test_eval_helper("(two-arg-greater-than 2 -3)", "t");
}

static void test_nthcdr()
{
    START_OF_TEST("nthcdr");
    test_eval_helper("(nthcdr 0 '(a b))", "(a b)");
    test_eval_helper("(nthcdr 1 '(a b))", "(b)");
    test_eval_helper("(nthcdr 2 '(a b))", "nil");
    test_eval_helper("(nthcdr 3 '(a b))", "nil");
}

static void test_push()
{
    START_OF_TEST("push");
    lisp_object_t l = NIL;
    lisp_object_t a = NIL;
    lisp_object_t b = NIL;
    lisp_object_t c = NIL;
    init_interpreter_for_tests();
    a = sym("a");
    b = sym("b");
    c = sym("c");
    l = cons(a, NIL);
    push(b, &l);
    push(c, &l);
    char *str = print_object(l);
    check(strcmp("(c b a)", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_vm_initialization()
{
    START_OF_TEST("vm_initialization");
    init_interpreter_for_tests();
    struct vm *vm = &interp->vm;
    free_interpreter();
}

static void test_compile3()
{
    START_OF_TEST("compile3");
    init_interpreter_for_tests();
    init_compiler();
}

static void test_vm_push_pop()
{
    START_OF_TEST("vm_push_pop");

    init_interpreter(1024 * 1024);

    lisp_object_t code_vector = parse1_wrapper("#(push foo push bar)");
    interp->vm.registers.code_vector = code_vector;
    vm_run(&interp->vm);

    check(eq(sym("bar"), vm_pop(&interp->vm)) != NIL, "stack1");
    check(eq(sym("foo"), vm_pop(&interp->vm)) != NIL, "stack2");
    check(interp->vm.top_of_data_stack == interp->vm.data_stack, "stack empty");

    free_interpreter();
}

static void test_vm_call_builtin()
{
    START_OF_TEST("vm_call_builtin");
    init_interpreter(1024 * 1024 * 4);

    lisp_object_t code_vector = parse1_wrapper("#(push foo push bar push 2 push cons call)");
    interp->vm.registers.code_vector = code_vector;
    vm_run(&interp->vm);

    char *str = print_object(vm_peek(&interp->vm));
    check(strcmp("(foo . bar)", str) == 0, "ok");
    free(str);

    free_interpreter();
}

static lisp_object_t make_function(lisp_object_t env, lisp_object_t code_vector, lisp_object_t arg_info)
{
    lisp_object_t fn = allocate_function();
    struct lisp_function *fnptr = LispFunctionPtr(fn);
    fnptr->kind = interp->syms.lambda;
    fnptr->actual_function = List(env, arg_info, code_vector);
    lisp_object_t sym = gensym();
    struct symbol *symptr = SymbolPtr(sym);
    symptr->function = fn;
    return sym;
}

static void test_vm_inst_call_lambda()
{
    START_OF_TEST("vm_inst_call_lambda");
    init_interpreter(1024 * 1024 * 4);
    /* The function */
    lisp_object_t arg_info = parse1_wrapper("#(nil 2)");
    lisp_object_t symbol = make_function(NIL, parse1_wrapper("#()"), arg_info);
    /* Set up the stack */
    vm_inst_push(&interp->vm, sym("foo"));
    vm_inst_push(&interp->vm, sym("bar"));
    vm_inst_push(&interp->vm, LispInt(2));
    vm_inst_push(&interp->vm, symbol);
    /* Call */
    vm_inst_call(&interp->vm);
    /* Check */
    lisp_object_t env = interp->vm.registers.environment;
    char *str = print_object(env);
    check(vectorp(env) != NIL, "vector on top of stack");
    check(svref_c(env, 0) == NIL, "parent env is NIL");
    check(svref_c(env, 1) == sym("foo"), "first arg");
    check(svref_c(env, 2) == sym("bar"), "second arg");
    free(str);
    free_interpreter();
}

static void test_vm_inst_get_set()
{
    START_OF_TEST("vm_inst_get_set");
    init_interpreter(1024 * 1024 * 4);

    lisp_object_t symbol = make_function(parse1_wrapper("#(nil bof xyz)"), parse1_wrapper("#(get 0 1 ret)"), parse1_wrapper("#(nil 2)"));

    vm_inst_push(&interp->vm, sym("foo"));
    vm_inst_push(&interp->vm, sym("bar"));
    vm_inst_push(&interp->vm, LispInt(2));
    vm_inst_push(&interp->vm, symbol);
    vm_inst_call(&interp->vm);

    vm_inst_get(&interp->vm, 0, LispInt(1));
    check(vm_pop(&interp->vm) == sym("foo"), "get");
    vm_inst_get(&interp->vm, LispInt(1), LispInt(2));
    check(vm_pop(&interp->vm) == sym("xyz"), "get closure env");

    vm_inst_push(&interp->vm, sym("baz"));
    vm_inst_set(&interp->vm, 0, LispInt(1));
    vm_inst_get(&interp->vm, 0, LispInt(1));
    check(vm_pop(&interp->vm) == sym("baz"), "set");

    vm_inst_push(&interp->vm, sym("boo"));
    vm_inst_set(&interp->vm, LispInt(1), LispInt(2));
    vm_inst_get(&interp->vm, LispInt(1), LispInt(2));
    check(vm_pop(&interp->vm) == sym("boo"), "set closure env");

    free_interpreter();
}

static void test_vm_function()
{
    START_OF_TEST("vm_function");
    init_interpreter(1024 * 1024 * 4);

    lisp_object_t lambda_code = parse1_wrapper("#(push hello get 0 1 push 2 push cons call ret)");
    /* Make the above code the function value of a symbol */
    lisp_object_t fn = allocate_function();
    struct lisp_function *fnptr = LispFunctionPtr(fn);
    fnptr->kind = interp->syms.lambda;
    lisp_object_t arg_info = parse1_wrapper("#(nil 1)");
    fnptr->actual_function = List(NIL, arg_info, lambda_code);
    struct symbol *symptr = SymbolPtr(sym("foo"));
    symptr->function = fn;

    /* Some code to call the function */
    lisp_object_t calling_code = parse1_wrapper("#(push world push 1 push placeholder call)");
    svref_set(calling_code, LispInt(5), fn);

    /* Run the code */
    interp->vm.registers.code_vector = calling_code;
    vm_run(&interp->vm);

    /* Check the result */
    lisp_object_t result = vm_peek(&interp->vm);
    char *str = print_object(result);
    check(strcmp("(hello . world)", str) == 0, "ok");
    free(str);

    free_interpreter();
}

static void test_vm_inst_jmp()
{
    START_OF_TEST("vm_inst_jmp");
    init_interpreter_for_tests();

    lisp_object_t code = parse1_wrapper("#(jmp 4 push foo push bar)");
    interp->vm.registers.code_vector = code;
    vm_run(&interp->vm);

    lisp_object_t top = vm_pop(&interp->vm);
    char *str = print_object(top);
    check(strcmp("bar", str) == 0, "ok");
    check(interp->vm.top_of_data_stack == interp->vm.data_stack, "stack empty");

    free(str);
    free_interpreter();
}

static void test_vm_inst_jmp_if_nil1()
{
    START_OF_TEST("vm_inst_jmp_if_nil1");
    init_interpreter_for_tests();

    lisp_object_t code = parse1_wrapper("#(push nil jmp-if-nil 6 push foo push bar)");
    interp->vm.registers.code_vector = code;
    vm_run(&interp->vm);

    lisp_object_t top = vm_pop(&interp->vm);
    char *str = print_object(top);
    check(strcmp("bar", str) == 0, "ok");
    check(interp->vm.top_of_data_stack == interp->vm.data_stack, "stack empty");

    free(str);
    free_interpreter();
}

static void test_vm_inst_jmp_if_nil2()
{
    START_OF_TEST("vm_inst_jmp_if_nil2");
    init_interpreter_for_tests();

    lisp_object_t code = parse1_wrapper("#(push abc jmp-if-nil 6 push foo push bar)");
    interp->vm.registers.code_vector = code;
    vm_run(&interp->vm);

    lisp_object_t top = vm_pop(&interp->vm);
    char *str = print_object(top);
    check(strcmp("bar", str) == 0, "ok");
    check(interp->vm.top_of_data_stack >= interp->vm.data_stack, "stack not empty");
    top = vm_pop(&interp->vm);
    str = print_object(top);
    check(strcmp("foo", str) == 0, "ok");

    free(str);
    free_interpreter();
}

static void test_keywords()
{
    START_OF_TEST("keywords");
    test_eval_helper(":foo", ":foo");
    free_interpreter();
}

static void test_vm_inst_set_tag()
{
    START_OF_TEST("vm_inst_set_tag");
    init_interpreter_for_tests();
    // lisp_object_t code = parse1_wrapper("#(set-tag bof 5 tag-push foo push bar)");
    vm_inst_set_tag(&interp->vm, sym("bof"), LispInt(4));
    char *str = print_object(interp->vm.registers.tags);
    check(strcmp("(#(bof 4 0))", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_unquote_splice_nil()
{
    START_OF_TEST("unquote_splice_nil");
    init_interpreter_for_tests();
    lisp_object_t result = test_eval_string_helper("`(foo ,@(progn nil))");
    char *str = print_object(result);
    check(strcmp("(foo)", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_vm_inst_tag_jmp()
{
    START_OF_TEST("vm_inst_tag_jmp");
    init_interpreter_for_tests();

    struct vm *vm = &interp->vm;
    vm->registers.code_vector = parse1_wrapper("#(foo bar baz quux boof)");
    vm->registers.instruction_pointer = 0;
    vm->registers.environment = NIL;
    vm->registers.tags = NIL;
    vm_inst_set_tag(vm, sym("bof"), LispInt(4));

    lisp_object_t expected_code_vector = vm->registers.code_vector;
    lisp_object_t expected_instruction_pointer = LispInt(4);

    /* As if a function call happened */
    *vm->call_stack_pointer = vm->registers;
    vm->call_stack_pointer++;
    vm->registers.code_vector = parse1_wrapper("#(1 2 3 4 5)");
    vm->registers.instruction_pointer = 0;
    vm->registers.environment = NIL;
    vm->registers.tags = NIL;

    vm_inst_tag_jmp(vm, sym("bof"));
    check(vm->registers.code_vector == expected_code_vector, "code vector");
    check(vm->registers.instruction_pointer == expected_instruction_pointer, "instruction pointer");

    free_interpreter();
}

static void test_vm_inst_raise()
{
    START_OF_TEST("vm_inst_raise");
    init_interpreter_for_tests();
    struct vm *vm = &interp->vm;
    vm->registers.code_vector = parse1_wrapper("#(foo bar baz quux boof)");
    vm->registers.instruction_pointer = 0;
    vm->registers.environment = NIL;
    vm->registers.tags = NIL;
    vm_inst_set_tag(vm, sym("bof"), LispInt(4));
    lisp_object_t expected_code_vector = vm->registers.code_vector;
    lisp_object_t expected_instruction_pointer = LispInt(4);

    /* As if a function call happened */
    *vm->call_stack_pointer = vm->registers;
    vm->call_stack_pointer++;
    vm->registers.code_vector = parse1_wrapper("#(1 2 3 4 5)");
    vm->registers.instruction_pointer = 0;
    vm->registers.environment = NIL;
    vm->registers.tags = NIL;

    /* raise is invoked like a function call */
    vm_inst_push(vm, sym("bof")); /* tag */
    vm_inst_push(vm, sym("return-value")); /* return value */
    vm_inst_push(vm, LispInt(2)); /* argcount */

    vm_inst_raise(vm);

    check(vm_peek(vm) == sym("return-value"), "return value");
    check(vm->registers.code_vector == expected_code_vector, "code vector");
    check(vm->registers.instruction_pointer == expected_instruction_pointer, "instruction pointer");

    free_interpreter();
}

void test_eval_function_pointer()
{
    START_OF_TEST("eval_function_pointer");
    init_interpreter_for_tests();
    lisp_object_t result = test_eval_string_helper("0x123400");
    char *str = print_object(result);
    check(strcmp("0x123400", str) == 0, "ok");
    free(str);
    free_interpreter();
}

void test_native_pointer()
{
    START_OF_TEST("test_native_pointer");
    init_interpreter_for_tests();
    lisp_object_t result = test_eval_string_helper("#p0x12340");
    check(result = 0x12340 | NATIVE_POINTER_TYPE, "tag");
    char *str = print_object(result);
    check(strcmp("#p0x12340", str) == 0, "print");
    free(str);
    free_interpreter();
}

int main(int argc, char **argv)
{
    test_skip_whitespace();
    test_comments();
    test_parse_integer();
    test_parse_large_integer();
    test_parse_negative_integer();
    test_parse_large_negative_integer();
    test_integer_too_large();
    test_integer_too_negative();
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
    test_read_empty_list();
    test_read_empty_list_in_list();
    test_strings();
    test_print_empty_cons();
    test_symbol_pointer();
    test_parse_symbol();
    test_parse_multiple_symbols();
    test_parse_list_of_symbols();
    test_parse_string();
    test_parse_string_with_escape_characters();
    test_parse_list_of_strings();
    test_eq();
    test_parse_multiple_objects();
    test_parse_handle_eof();
    test_parse_quote();
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
    test_sym();
    test_evalquote();
    test_eval();
    test_load();
    test_set();
    test_rplaca();
    test_rplacd();
    test_rest_args();
    test_plus();
    test_minus();
    test_times();
    test_divide();
    test_read_token();
    test_numeric_equals();
    test_parse_function_pointer();
    test_print_function_pointer();
    test_call_function_pointer();
    test_integer_bug();
    test_condition_case();
    test_functionp();
    test_print_function();
    test_unbound_variable();
    test_plist();
    test_defmacro();
    test_unquote_splice();
    test_optional_arguments();
    test_progn();
    test_tagbody();
    test_tagbody_bug();
    test_tagbody_returns_nil();
    test_tagbody_condition_case();
    test_let();
    test_macroexpand1();
    test_macroexpand();
    test_macro_inside_quasiquote();
    test_macroexpand_all_if();
    test_macroexpand_all_progn();
    test_macroexpand_all_lambda();
    test_macroexpand_all_tagbody();
    test_macroexpand_all_prog();
    test_macroexpand_all_set();
    test_macroexpand_all_let();
    test_macroexpand_all_defmacro();
    test_macroexpand_all_condition_case();
    test_lambda_implicit_progn();
    test_lisp_heap_cons();
    test_lisp_heap_copy_single_object();
    test_lisp_heap_gc_simple();
    test_vector_builtins();
    test_non_symbol_in_function_position();
    test_type_of();
    test_comma_not_inside_backquote();
    test_string_equalp();
    test_length_builtin();
    test_parse_empty_vector();
    test_quasiquote_bug();
    test_apply();
    test_parse_function();
    test_nonexistent_function();
    test_unquote_splice_bug();
    test_gensym();
    test_varargs_list();
    test_block();
    test_nil_block();
    test_nested_block();
    test_nested_nil_block();
    test_compile_bug();
    test_macroexpand_function_lambda();
    test_set_symbol_function();
    test_set_symbol_value();
    test_compile_condition_case();
    test_if();
    test_less_than();
    test_greater_than();
    test_nthcdr();
    test_push();
    test_vm_initialization();
    test_vm_push_pop();
    test_vm_call_builtin();
    test_vm_inst_call_lambda();
    test_vm_inst_get_set();
    test_vm_function();
    test_vm_inst_jmp();
    test_vm_inst_jmp_if_nil1();
    test_vm_inst_jmp_if_nil2();
    test_keywords();
    test_vm_inst_set_tag();
    test_vm_inst_tag_jmp();
    test_unquote_splice_nil();
    test_vm_inst_raise();
    test_eval_function_pointer();
    test_native_pointer();
    if (fail_count)
        printf("%d checks failed\n", fail_count);
    else
        printf("All tests successful\n");
    return fail_count;
}
