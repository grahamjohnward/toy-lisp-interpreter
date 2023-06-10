#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lisp.h"
#include "string_buffer.h"
#include "text_stream.h"

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

static void test_skip_whitespace()
{
    test_name = "skip_whitespace";
    char *test_string = "  hello";
    struct text_stream ts;
    text_stream_init_str(&ts, test_string);
    skip_whitespace(&ts);
    check(text_stream_peek(&ts) == 'h', "next char");
}

static void test_comments()
{
    test_name = "comments";
    char *test_string = "; This is a comment";
    struct text_stream ts;
    text_stream_init_str(&ts, test_string);
    skip_whitespace(&ts);
    check(text_stream_eof(&ts), "eof");
}

static void test_parse_integer()
{
    test_name = "parse_integer";
    char *test_string = "13";
    uint64_t result = parse1_wrapper(test_string);
    check(result == 13 << 4, "value");
}

static void test_parse_large_integer()
{
    test_name = "parse_large_integer";
    char *test_string = "1152921504606846975";
    uint64_t result = parse1_wrapper(test_string);
    check(result == 1152921504606846975 << 4, "value");
    check(integerp((lisp_object_t)result) != NIL, "integerp");
}

static void test_parse_negative_integer()
{
    test_name = "parse_negative_integer";
    char *test_string = "-498";
    uint64_t result = parse1_wrapper(test_string);
    check(result == -498 * 16, "value");
}

static void test_parse_large_negative_integer()
{
    test_name = "parse_large_negative_integer";
    char *test_string = "-1152921504606846976";
    uint64_t result = parse1_wrapper(test_string);
    check(result == ((uint64_t)-1152921504606846976 * 16), "value");
    check(integerp((lisp_object_t)result) != NIL, "integerp");
}

static void test_integer_too_large()
{
    test_name = "test_integer_too_large";
    char *test_string = "1152921504606846976";
    /* Calls abort() */
    // int64_t result = parse_integer_wrapper(&test_string);
}

static void test_integer_too_negative()
{
    test_name = "test_integer_too_negative";
    char *test_string = "-1152921504606846977";
    /* Calls abort() */
    // int64_t result = parse_integer_wrapper(&test_string);
}

static void test_parse_single_integer_list()
{
    test_name = "parse_single_integer_list";
    char *test_string = "(14)";
    init_interpreter(256);
    lisp_object_t result = parse1_wrapper(test_string);
    check(consp(result), "consp");
    lisp_object_t result_car = car(result);
    check(integerp(result_car), "car is int");
    check(result_car == 14, "car value");
    lisp_object_t result_cdr = cdr(result);
    check(NIL == result_cdr, "cdr is null");
    free_interpreter();
}

static void test_parse_integer_list()
{
    test_name = "parse_integer_list";
    char *test_string = "(23 71)";
    init_interpreter(32768);
    lisp_object_t result = parse1_wrapper(test_string);
    check(consp(result), "consp");
    lisp_object_t result_car = car(result);
    check(integerp(result_car), "car is int");
    check(result_car == 23 << 4, "car value");
    lisp_object_t result_cdr = cdr(result);
    check(NIL != result_cdr, "cdr is not null");
    check(consp(result_cdr), "cdr is a pair");
    lisp_object_t cadr = car(result_cdr);
    check(integerp(cadr), "cadr is int");
    check(cadr == 71 << 4, "cadr value");
    free_interpreter();
}

static void test_parse_dotted_pair_of_integers()
{
    test_name = "parse_dotted_pair_of_integers";
    char *test_string = "(45 . 123)";
    init_interpreter(32768);
    lisp_object_t result = parse1_wrapper(test_string);
    check(consp(result), "consp");
    check(integerp(car(result)), "car is int");
    check(integerp(cdr(result)), "cdr is int");
    check(car(result) == 45 << 4, "car value");
    check(cdr(result) == 123 << 4, "cdr value");
    free_interpreter();
}

static void test_string_buffer()
{
    test_name = "string_buffer";
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
    test_name = "print_integer";
    char *test_string = "93";
    init_interpreter(32768);
    lisp_object_t obj = parse1_wrapper(test_string);
    char *result = print_object(obj);
    check(strcmp("93", result) == 0, "string value");
    free(result);
    free_interpreter();
}

static void test_print_single_integer_list()
{
    test_name = "print_single_integer_list";
    char *test_string = "(453)";
    init_interpreter(32768);
    lisp_object_t obj = parse1_wrapper(test_string);
    char *result = print_object(obj);
    check(strcmp("(453)", result) == 0, "string value");
    free(result);
    free_interpreter();
}

static void test_print_integer_list()
{
    test_name = "print_integer_list";
    char *test_string = "(240 -44 902)";
    init_interpreter(32768);
    lisp_object_t obj = parse1_wrapper(test_string);
    char *result = print_object(obj);
    check(strcmp("(240 -44 902)", result) == 0, "string value");
    free(result);
    free_interpreter();
}

static void test_print_dotted_pair()
{
    test_name = "print_dotted_pair";
    char *test_string = "(65 . 185)";
    init_interpreter(32768);
    lisp_object_t obj = parse1_wrapper(test_string);
    char *result = print_object(obj);
    check(strcmp("(65 . 185)", result) == 0, "string value");
    free(result);
    free_interpreter();
}

static void test_print_complex_list()
{
    test_name = "print_complex_list";
    char *test_string = "(1 (2 3 4 (5 (6 7 8 (9 . 0)))))";
    init_interpreter(32768);
    lisp_object_t obj = parse1_wrapper(test_string);
    char *result = print_object(obj);
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
    char *test_string = "nil";
    init_interpreter(32768);
    lisp_object_t obj = parse1_wrapper(test_string);
    check(obj == NIL, "is nil");
    char *result = print_object(obj);
    check(strcmp("nil", result) == 0, "print nil");
    free(result);
    free_interpreter();
}

static void test_read_and_print_t()
{
    test_name = "read_and_print_t";
    char *test_string = "t";
    init_interpreter(32768);
    lisp_object_t obj = parse1_wrapper(test_string);
    check(obj == T, "is T");
    char *result = print_object(obj);
    check(strcmp("t", result) == 0, "print t");
    free(result);
    free_interpreter();
}

static void test_read_empty_list()
{
    test_name = "read_empty_list";
    init_interpreter(32768);
    char *test_string = "()";
    lisp_object_t result = parse1_wrapper(test_string);
    check(result == NIL, "is nil");
    free_interpreter();
}

static void test_read_empty_list_in_list()
{
    test_name = "read_empty_list_in_list";
    init_interpreter(32768);
    char *test_string = "(abc () xyz)";
    lisp_object_t result = parse1_wrapper(test_string);
    char *str = print_object(result);
    check(strcmp(str, "(abc nil xyz)") == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_strings()
{
    test_name = "strings";
    init_interpreter(32768);
    lisp_object_t s1 = allocate_string(6, "hello");
    lisp_object_t s2 = allocate_string(6, "hello");
    lisp_object_t s3 = allocate_string(7, "oohaah");
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
    test_name = "print_empty_cons";
    init_interpreter(32768);
    lisp_object_t empty = cons(NIL, NIL);
    char *str = print_object(empty);
    check(strcmp("(nil)", str) == 0, "(nil)");
    free(str);
    free_interpreter();
}

static void test_symbol_pointer()
{
    test_name = "symbol_pointer";
    lisp_object_t obj_without_tag = 0x123400;
    lisp_object_t tagged_obj = obj_without_tag | SYMBOL_TYPE;
    struct symbol *ptr = SymbolPtr(tagged_obj);
    check((unsigned long)obj_without_tag == (unsigned long)ptr, "correct pointer");
}

static void test_parse_symbol()
{
    test_name = "parse_symbol";
    char *test_string = "foo";
    init_interpreter(32768);
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
    test_name = "parse_multiple_symbols";
    char *s1 = "foo";
    init_interpreter(32768);
    interp->symbol_table = NIL;
    lisp_object_t sym1 = parse1_wrapper(s1);
    char *s2 = "bar";
    lisp_object_t sym2 = parse1_wrapper(s2);
    char *str = print_object(interp->symbol_table);
    check(strcmp("(bar foo)", str) == 0, "symbol table looks right");
    free(str);
    char *s3 = "bar";
    lisp_object_t sym3 = parse1_wrapper(s3);
    check(eq(sym2, sym3) == T, "symbols eq");
    str = print_object(interp->symbol_table);
    check(strcmp("(bar foo)", str) == 0, "symbol table looks right(2)");
    free(str);
    free_interpreter();
}

static void test_parse_list_of_symbols()
{
    test_name = "parse_list_of_symbols";
    char *test_string = "(hello you are nice)";
    init_interpreter(32768);
    lisp_object_t result = parse1_wrapper(test_string); // bad
    check(consp(result) != NIL, "consp");
    check(symbolp(car((result))) != NIL, "first symbolp");
    char *str = print_object(result);
    check(strcmp("(hello you are nice)", str) == 0, "prints ok");
    free(str);
    free_interpreter();
}

static void test_parse_string()
{
    test_name = "parse_string";
    init_interpreter(32768);
    char *string = "\"hello\"";
    lisp_object_t obj = parse_string_wrapper(string);
    check(stringp(obj), "stringp");
    size_t len;
    char *str;
    get_string_parts(obj, &len, &str);
    check(len == 5, "length");
    check(strcmp("hello", str) == 0, "value");
    char *str2 = print_object(obj);
    check(strcmp("hello", str2) == 0, "print_object");
    free(str2);
    free_interpreter();
}

static void test_parse_string_with_escape_characters()
{
    test_name = "parse_string_with_escape_characters";
    init_interpreter(32768);
    char *string = "\"he\\\"llo\\n\\t\\r\"";
    lisp_object_t obj = parse_string_wrapper(string);
    check(stringp(obj), "stringp");
    size_t len;
    char *str;
    get_string_parts(obj, &len, &str);
    check(len == 9, "length");
    check(strcmp("he\"llo\n\t\r", str) == 0, "value");
    free_interpreter();
}

static void test_parse_list_of_strings()
{
    test_name = "parse_list_of_strings";
    init_interpreter(32768);
    char *string = "(\"hello\" \"world\")";
    lisp_object_t obj = parse1_wrapper(string);
    check(consp(obj), "list returned");
    lisp_object_t s1 = car(obj);
    check(stringp(s1), "first element is string");
    char *s1str = print_object(s1);
    check(strcmp("hello", s1str) == 0, "first element ok");
    lisp_object_t s2 = car(cdr(obj));
    char *s2str = print_object(s2);
    check(strcmp("world", s2str) == 0, "second element ok");
    free(s1str);
    free(s2str);
    check(stringp(s2), "second element is string");
}

static void test_eq()
{
    test_name = "eq";
    check(eq(0, 0) == T, "(eq 0 0) is t");
    check(eq(1, 1) == T, "(eq 1 1) is t");
    check(eq(NIL, NIL) == T, "(eq nil nil) is t");
    check(eq(T, T) == T, "(eq t t) is t");
    check(eq(0, 0) != NIL, "(eq 0 0) is not nil");
    check(eq(1, 1) != NIL, "(eq 1 1) is not nil");
    check(eq(NIL, NIL) != NIL, "(eq nil nil) is not nil");
    check(eq(T, T) != NIL, "(eq t t) is not nil");
}

/* The parse updates the pointer passed to it.
   This test is to say that we think this is OK */
static void test_parser_advances_pointer()
{
    /* test_name = "parser_advances_pointer";
     char *s1 = "foo";
     char *before = s1;
     init_interpreter(256);
     lisp_object_t sym1 = parse1(&s1);
     check(s1 - before == 3, "pointer advanced");
     free_interpreter();*/
}

static void test_parse_multiple_objects_callback(void *data, lisp_object_t obj)
{
    print_object_to_buffer(obj, (struct string_buffer *)data);
}

static void test_parse_multiple_objects()
{
    test_name = "parse_multiple_objects";
    char *test_string = "foo bar";
    init_interpreter(32768);
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
    test_name = "parse_handle_eof";
    char *test_string = "foo bar\n";
    init_interpreter(32768);
    struct string_buffer sb;
    string_buffer_init(&sb);
    int count = 0;
    parse_wrapper(test_string, test_parse_handle_eof_callback, &count);
    check(count == 2, "two objects");
    free_interpreter();
}

static void test_parse_quote()
{
    test_name = "parse_quote";
    char *test_string = "'FOO";
    init_interpreter(32768);
    lisp_object_t result = parse1_wrapper(test_string);
    struct string_buffer sb;
    string_buffer_init(&sb);
    print_object_to_buffer(result, &sb);
    char *str = string_buffer_to_string(&sb);
    string_buffer_free_links(&sb);
    check(strcmp("'FOO", str) == 0, "parse quote");
    free(str);
    free_interpreter();
}

static void test_vector_initialization()
{
    test_name = "vector_initialization";
    init_interpreter(32768);
    lisp_object_t v = allocate_vector(3 << 4);
    check(eq(svref(v, 0), NIL) != NIL, "first element nil");
    check(eq(svref(v, 1), NIL) != NIL, "second element nil");
    check(eq(svref(v, 2), NIL) != NIL, "third element nil");
    free_interpreter();
}

static void test_vector_svref()
{
    test_name = "vector_svref";
    init_interpreter(32768);
    char *symbol_text = "foo";
    lisp_object_t sym = parse1_wrapper(symbol_text);
    lisp_object_t v = allocate_vector(3 << 4);
    char *list_text = "(a b c)";
    lisp_object_t list = parse1_wrapper(list_text);
    svref_set(v, 0, 14);
    svref_set(v, 1 << 4, sym);
    svref_set(v, 2 << 4, list);
    check(eq(svref(v, 0), 14) != NIL, "first element");
    check(eq(svref(v, 1 << 4), sym) != NIL, "second element");
    check(eq(svref(v, 2 << 4), list) != NIL, "third element");
    free_interpreter();
}

static void test_parse_vector()
{
    test_name = "parse_vector";
    char *text = "#(a b c)";
    init_interpreter(32768);
    lisp_object_t result = parse1_wrapper(text);
    check(vectorp(result) == T, "vectorp");
    char *a_text = "a";
    lisp_object_t sym_a = parse1_wrapper(a_text);
    check(eq(sym_a, svref(result, 0)) == T, "first element");
    char *b_text = "b";
    lisp_object_t sym_b = parse1_wrapper(b_text);
    check(eq(sym_b, svref(result, 1 << 4)) == T, "second element");
    char *c_text = "c";
    lisp_object_t sym_c = parse1_wrapper(c_text);
    check(eq(sym_c, svref(result, 2 << 4)) == T, "third element");
    free_interpreter();
}

static void test_print_vector()
{
    test_name = "print_vector";
    char *text = "#(a b c)";
    init_interpreter(32768);
    lisp_object_t result = parse1_wrapper(text);
    char *str = print_object(result);
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
    init_interpreter(32768);
    char *text1 = "((X . SHAKESPEARE) (Y . (THE TEMPEST)))";
    lisp_object_t obj = parse1_wrapper(text1);
    char *str = print_object(obj);
    check(strcmp("((X . SHAKESPEARE) (Y THE TEMPEST))", str) == 0, "");
    free(str);
    free_interpreter();
}

static void test_sublis()
{
    test_name = "test_sublis";
    init_interpreter(32768);
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
    test_name = "null";
    check(null(NIL) != NIL, "nil");
    check(null(T) == NIL, "t");
}

static void test_append()
{
    test_name = "append";
    init_interpreter(32768);
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
    test_name = "member";
    init_interpreter(32768);
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
    test_name = "assoc";
    init_interpreter(32768);
    char *text1 = "((A . (M N)) (B . (car X)) (C . (quote M)) (C . (cdr x)))";
    char *text2 = "B";
    char *text3 = "X";
    lisp_object_t alist = parse1_wrapper(text1);
    lisp_object_t b = parse1_wrapper(text2);
    lisp_object_t x = parse1_wrapper(text3);
    lisp_object_t result = assoc(b, alist);
    char *str = print_object(result);
    check(strcmp("(B car X)", str) == 0, "match found");
    free(str);
    check(assoc(x, alist) == NIL, "match not present");
    free_interpreter();
}

static void test_pairlis()
{
    test_name = "pairlis";
    init_interpreter(32768);
    char *text1 = "(A B C)";
    char *text2 = "(U V W)";
    char *text3 = "((D . X) (E . Y))";
    lisp_object_t x = parse1_wrapper(text1);
    lisp_object_t y = parse1_wrapper(text2);
    lisp_object_t a = parse1_wrapper(text3);
    lisp_object_t result = pairlis(x, y, a);
    char *str = print_object(result);
    check(strcmp("((A . U) (B . V) (C . W) (D . X) (E . Y))", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_sym()
{
    test_name = "sym";
    init_interpreter(32768);
    lisp_object_t x1 = sym("x");
    lisp_object_t x2 = sym("x");
    lisp_object_t y = sym("y");
    check(eq(x1, x2) != NIL, "(eq x1 x2)");
    check(eq(x1, y) == NIL, "(not (eq x1 y))");
    check(eq(x2, y) == NIL, "(not (eq x2 y))");
    free_interpreter();
}

static void test_evalquote_helper(char *fnstr, char *exprstr, char *expected)
{
    init_interpreter(32768);
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
    test_name = "evalquote";
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
    lisp_object_t expr = parse1_wrapper(exprstr);
    lisp_object_t result = eval_toplevel(expr);
    return result;
}

static void test_eval_helper(char *exprstr, char *expectedstr)
{
    init_interpreter(65536);
    char *exprstr_save = exprstr;
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
    char *stuff = string_buffer_to_string(&sb);
    check(ok, stuff);
    free(stuff);
    free(resultstr);
    string_buffer_free_links(&sb);
    free_interpreter();
}

static void test_eval()
{
    test_name = "eval";
    /*    test_eval_helper("t", "t");
        test_eval_helper("3", "3");
        test_eval_helper("(cons (quote A) (quote B))", "(A . B)");
        test_eval_helper("(cond ((eq (car (cons (quote A) nil)) (quote A)) (quote OK)))", "OK");
        test_eval_helper("(cond ((eq (car (cons (quote A) nil)) (quote B)) (quote BAD)) (t (quote OK)))", "OK");*/
    test_eval_helper("(funcall (function (lambda (X) (car X))) (cons (quote A) (quote B)))", "A");
}

static void test_defun()
{
    test_name = "defun";
    init_interpreter(32768);
    lisp_object_t result1 = test_eval_string_helper("(defun foo (x) (cons x (quote bar)))");
    lisp_object_t result2 = test_eval_string_helper("(foo 14)");
    char *str = print_object(result2);
    check(strcmp("(14 . bar)", str) == 0, "result");
    free(str);
    free_interpreter();
}

static void test_load1()
{
    test_name = "load";
    lisp_object_t result1 = test_eval_string_helper("(load \"/home/graham/toy-lisp-interpreter/test-load.lisp\")");
    check(result1 == T, "load returns T");
    lisp_object_t result2 = test_eval_string_helper("(test1 (quote there))");
    char *str = print_object(result2);
    check(strcmp("(hello . there)", str) == 0, "result of test1");
    free(str);
}

static void test_load()
{
    init_interpreter(65536);
    top_of_stack = (lisp_object_t *)get_rbp(1);
    test_load1();
    free_interpreter();
}

lisp_object_t test_fn(lisp_object_t a, lisp_object_t b)
{
    return cons(b, a);
}

static void test_set()
{
    test_name = "set";
    test_eval_helper("(funcall (function (lambda (x) (prog () (set 'x 14) (return x)))) 12)", "14");
}

static void test_prog()
{
    test_name = "prog";
    test_eval_helper("(funcall (function (lambda (x) (prog (y) (set 'y 12) bof (set 'x 36) boo (return (cons x y))))) 14)", "(36 . 12)");
}

static void test_rplaca()
{
    test_name = "rplaca";
    test_eval_helper("(prog (x) (set 'x (cons 12 13)) (rplaca x 4) (return (car x)))", "4");
}

static void test_rplacd()
{
    test_name = "rplacd";
    test_eval_helper("(prog (x) (set 'x (cons 3 5)) (rplacd x 7) (return (cdr x)))", "7");
}

static void test_rest_args()
{
    test_name = "rest_args";
    init_interpreter(32768);
    top_of_stack = (lisp_object_t *)get_rbp(1);
    char *teststr = "(defun foo (a b &rest c) (cons c (cons b a)))";
    eval_toplevel(parse1_wrapper(teststr));
    teststr = "(foo 1 2 3)";
    lisp_object_t result = eval_toplevel(parse1_wrapper(teststr));
    char *str = print_object(result);
    check(strcmp("(3 2 . 1)", str), "result");
    free(str);
}

static void test_plus()
{
    test_name = "plus";
    test_eval_helper("(two-arg-plus 3 4)", "7");
}

static void test_minus()
{
    test_name = "minus";
    test_eval_helper("(two-arg-minus 7 4)", "3");
}

static void test_times()
{
    test_name = "times";
    test_eval_helper("(two-arg-times 3 4)", "12");
    test_eval_helper("(two-arg-times -3 4)", "-12");
    test_eval_helper("(two-arg-times 65536 65536)", "4294967296");
}

static void test_return_from_prog()
{
    test_name = "return_from_prog";
    test_eval_helper("(prog (x) (set 'x 12) (cond ((eq x 12) (return 'twelve)) (t nil)) 'bof)", "twelve");
}

static void test_read_token()
{
    test_name = "read_token";
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
    test_name = "numeric_equals";
    test_eval_helper("(= 3 3)", "t");
    test_eval_helper("(= 4 3)", "nil");
}

static void test_parse_function_pointer()
{
    test_name = "parse_function_pointer";
    char *teststr = "0x1234";
    lisp_object_t result = parse1_wrapper(teststr);
    check(function_pointer_p(result) != NIL, "function_pointer_p");
    check(FunctionPtr(result) == (void (*)())0x1234, "value");
}

static void test_print_function_pointer()
{
    test_name = "print_function_pointer";
    lisp_object_t fp = FUNCTION_POINTER_TYPE | (0x1234 << 4);
    char *str = print_object(fp);
    check(strcmp("0x1234", str) == 0, "0x123400");
    free(str);
}

static void test_call_function_pointer()
{
    test_name = "call_function_pointer";
    /* This actually works if you can get the address right */
    // test_eval_helper("((built-in-function 0x404f90 2) 3 4)", "(3 . 4)");
}

static void test_integer_bug()
{
    test_name = "integer_bug";
    test_eval_helper("(two-arg-minus (two-arg-minus 123 12) 312312)", "-312201");
}

static void test_return_outside_prog()
{
    test_name = "return_outside_prog";
    init_interpreter(32768);
    lisp_object_t result1 = test_eval_string_helper("(defun foo (x) (return (cons 'returned x)))");
    lisp_object_t result2 = test_eval_string_helper("(prog (x) (set 'x 12) (return (cons 'aha (foo x))))");
    char *result2str = print_object(result2);
    check(strcmp("(aha returned . 12)", result2str) == 0, "return value");
    free(result2str);
    free_interpreter();
}

static void test_prog_without_return()
{
    test_name = "prog_without_return";
    test_eval_helper("(prog (x y) (set 'x 14) (set 'y 12) (cons x y))", "nil");
}

static void test_condition_case()
{
    test_name = "condition_case";
    test_eval_helper("(condition-case e (raise 'ohno 14) (ohno (cons 'error-was e)) (didnt-happen 'frob))", "(error-was ohno . 14)");
}

static void test_functionp()
{
    test_name = "functionp";
    init_interpreter(32768);
    check(functionp(test_eval_string_helper("(function (lambda (x) (cons x x)))")) == T, "lambda t");
    check(functionp(test_eval_string_helper("(function 'cons)")) == T, "cons t");
    check(functionp(parse1_wrapper("foo")) == NIL, "symbol nil");
    check(functionp(test_eval_string_helper("14")) == NIL, "integer nil");
    free_interpreter();
}

static void test_print_function()
{
    test_name = "print_function";
    test_eval_helper("(function (lambda (x) (cons x x)))", "#<function>");
    test_eval_helper("(function 'cons)", "#<function>");
}

static void test_unbound_variable()
{
    test_name = "unbound_variable";
    test_eval_helper("(condition-case e (print x) (unbound-variable (cons 'ohdear e)))", "(ohdear unbound-variable . x)");
}

static void test_plist()
{
    test_name = "plist";
    test_eval_helper("(prog () (putprop 'foo 'greeting '(hello world)) (return (get 'foo 'greeting)))", "(hello world)");
}

static void test_defmacro()
{
    test_name = "defmacro";
    init_interpreter(65536);
    test_eval_string_helper("(defmacro if (test then else) `(cond (,test ,then) (t ,else)))");
    lisp_object_t result = test_eval_string_helper("(if (eq (car (cons 3 4)) 3) (two-arg-plus 9 9) 'bof)");
    check(result == 18 << 4, "test1");
    result = test_eval_string_helper("(if (eq (car (cons 3 4)) 4) (two-arg-plus 9 9) 'bof)");
    check(eq(result, sym("bof")) != NIL, "test2");
    free_interpreter();
}

static void test_unquote_splice()
{
    test_name = "unquote_splice";
    init_interpreter(65536);
    test_eval_string_helper("(defmacro when (test &body then) `(cond (,test (prog () ,@then)) (t nil)))");
    lisp_object_t result = test_eval_string_helper("(when (eq (car (cons 3 2)) 3) (print 'bof) 14)");
    free_interpreter();
}

static void test_optional_arguments()
{
    test_name = "optional_arguments";
    init_interpreter(32768);
    eval_toplevel(parse1_wrapper("(defun test (a &optional b) (cons 'hello (cons a (cons b 'foo))))"));
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

static void test_progn()
{
    test_name = "progn";
    init_interpreter(32768);
    test_eval_string_helper("(defun foo (x y) (progn (set 'x 12) (set 'y 13) (cons 12 13)))");
    lisp_object_t result = test_eval_string_helper("(foo 3 4)");
    char *str = print_object(result);
    check(strcmp(str, "(12 . 13)") == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_tagbody()
{
    test_name = "tagbody";
    init_interpreter(65536);
    test_eval_string_helper("(defun foo (x) (tagbody iterate (cond ((= x 0) (return 'done)) (t (progn (set 'x (two-arg-minus x 1)) (go iterate)))))))");
    lisp_object_t result = test_eval_string_helper("(foo 10)");
    char *str = print_object(result);
    check(strcmp("done", str) == 0, "ok");
    free(str);
}

static void test_tagbody_bug()
{
    test_name = "tagbody_bug";
    init_interpreter(32768);
    test_eval_string_helper("(defun test (x) (progn (tagbody (set 'x 14)) x))");
    lisp_object_t result = test_eval_string_helper("(test 2)");
    check(result == 14 << 4, "ok");
    free_interpreter();
}

static void test_tagbody_returns_nil()
{
    test_name = "tagbody_returns_nil";
    test_eval_helper("(tagbody 14)", "nil");
}

static void test_tagbody_condition_case()
{
    test_name = "tagbody_condition_case";
    init_interpreter(32768);
    test_eval_string_helper("(defun ooh () (tagbody (condition-case e (raise 'ohno) (ohno (go hello))) (return 'bad) hello (return 'hello)))");
    lisp_object_t result = test_eval_string_helper("(ooh)");
    char *str = print_object(result);
    check(strcmp("hello", str) == 0, "ok");
    free(str);
}

static void test_let()
{
    test_name = "let";
    test_eval_helper("(let ((a 3) (b (two-arg-plus 10 2)) (c 'frob) (d 14) x) (set 'd 8) (cons (two-arg-plus a b) (cons c (cons x d))))", "(15 frob nil . 8)");
}

static void test_macroexpand1()
{
    test_name = "macroexpand1";
    init_interpreter(32768);
    test_eval_string_helper("(defmacro ooh (x) `(aah ,x))");
    test_eval_string_helper("(defmacro aah (x) `(bar ,x))");
    lisp_object_t expr = parse1_wrapper("(ooh (frob))");
    lisp_object_t result = macroexpand1(expr, NIL);
    char *str = print_object(result);
    check(strcmp("((aah (frob)) . t)", str) == 0, "ok");
    free(str);
}

static void test_macroexpand()
{
    test_name = "macroexpand";
    init_interpreter(32768);
    test_eval_string_helper("(defmacro ooh (x) `(aah ,x))");
    test_eval_string_helper("(defmacro aah (x) `(bar ,x))");
    lisp_object_t expr = parse1_wrapper("(ooh (frob))");
    lisp_object_t result = macroexpand(expr, NIL);
    char *str = print_object(result);
    check(strcmp("(bar (frob))", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_macroexpand_all_cond()
{
    test_name = "macroexpand_all_cond";
    init_interpreter(65536);
    test_eval_string_helper("(defmacro ooh (x) `(aah ,x))");
    test_eval_string_helper("(defmacro aah (x) `(bar ,x))");
    lisp_object_t expr = parse1_wrapper("(cond (nil 'ooh) (t (ooh (frob))))");
    lisp_object_t result = macroexpand_all(expr);
    char *str = print_object(result);
    check(strcmp("(cond (nil 'ooh) (t (bar (frob))))", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_macroexpand_all_progn()
{
    test_name = "macroexpand_all_progn";
    init_interpreter(65536);
    test_eval_string_helper("(defmacro ooh (x) `(aah ,x))");
    test_eval_string_helper("(defmacro aah (x) `(bar ,x))");
    lisp_object_t expr = parse1_wrapper("(progn (ooh (frob)) (aah (hello)))");
    lisp_object_t result = macroexpand_all(expr);
    char *str = print_object(result);
    check(strcmp("(progn (bar (frob)) (bar (hello)))", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_macroexpand_all_lambda()
{
    test_name = "macroexpand_all_lambda";
    init_interpreter(65536);
    test_eval_string_helper("(defmacro ooh (x) `(aah ,x))");
    test_eval_string_helper("(defmacro aah (x) `(bar ,x))");
    lisp_object_t expr = parse1_wrapper("(lambda (x) (ooh (frob)) (aah (hello)))");
    lisp_object_t result = macroexpand_all(expr);
    char *str = print_object(result);
    check(strcmp("(lambda (x) (bar (frob)) (bar (hello)))", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_macroexpand_all_tagbody()
{
    test_name = "macroexpand_all_tagbody";
    init_interpreter(65536);
    test_eval_string_helper("(defmacro ooh (x) `(aah ,x))");
    test_eval_string_helper("(defmacro aah (x) `(bar ,x))");
    lisp_object_t expr = parse1_wrapper("(tagbody (ooh (frob)) foo (aah (hello)) (go foo))");
    lisp_object_t result = macroexpand_all(expr);
    char *str = print_object(result);
    check(strcmp("(tagbody (bar (frob)) foo (bar (hello)) (go foo))", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_macroexpand_all_prog()
{
    test_name = "macroexpand_all_prog";
    init_interpreter(65536);
    test_eval_string_helper("(defmacro ooh (x) `(aah ,x))");
    test_eval_string_helper("(defmacro aah (x) `(bar ,x))");
    lisp_object_t expr = parse1_wrapper("(prog (a b) (ooh (frob)) foo (aah (hello)) (go foo))");
    lisp_object_t result = macroexpand_all(expr);
    char *str = print_object(result);
    check(strcmp("(prog (a b) (bar (frob)) foo (bar (hello)) (go foo))", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_macroexpand_all_set()
{
    test_name = "macroexpand_all_set";
    init_interpreter(32768);
    test_eval_string_helper("(defmacro frob (x) 'x)");
    test_eval_string_helper("(defmacro aah (x) `(bar ,x))");
    lisp_object_t expr = parse1_wrapper("(set (frob) (aah (hello)))");
    lisp_object_t result = macroexpand_all(expr);
    char *str = print_object(result);
    check(strcmp("(set x (bar (hello)))", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_macroexpand_all_let()
{
    test_name = "macroexpand_all_let";
    init_interpreter(65536);
    test_eval_string_helper("(defmacro ooh (x) `(aah ,x))");
    test_eval_string_helper("(defmacro aah (x) `(bar ,x))");
    lisp_object_t expr = parse1_wrapper("(let ((a 14) (b (ooh y))) (ooh b))");
    lisp_object_t result = macroexpand_all(expr);
    char *str = print_object(result);
    check(strcmp("(let ((a 14) (b (bar y))) (bar b))", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_macroexpand_all_defun()
{
    test_name = "macroexpand_all_defun";
    init_interpreter(65536);
    test_eval_string_helper("(defmacro ooh (x) `(aah ,x))");
    test_eval_string_helper("(defmacro aah (x) `(bar ,x))");
    lisp_object_t expr = parse1_wrapper("(defun myfun (a b) (ooh a) (aah b))");
    lisp_object_t result = macroexpand_all(expr);
    char *str = print_object(result);
    check(strcmp("(defun myfun (a b) (bar a) (bar b))", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_macroexpand_all_defmacro()
{
    test_name = "macroexpand_all_defmacro";
    init_interpreter(65536);
    test_eval_string_helper("(defmacro ooh (x) `(aah ,x))");
    test_eval_string_helper("(defmacro aah (x) `(bar ,x))");
    lisp_object_t expr = parse1_wrapper("(defmacro mymacro (a b) (ooh a) (aah b))");
    lisp_object_t result = macroexpand_all(expr);
    char *str = print_object(result);
    check(strcmp("(defmacro mymacro (a b) (bar a) (bar b))", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_macroexpand_all_condition_case()
{
    test_name = "macroexpand_all_condition_case";
    init_interpreter(65536);
    test_eval_string_helper("(defmacro ooh (x) `(aah ,x))");
    test_eval_string_helper("(defmacro aah (x) `(bar ,x))");
    lisp_object_t expr = parse1_wrapper("(condition-case e (ooh 3) (ohno (aah 9)) (didnt-happen e))");
    lisp_object_t result = macroexpand_all(expr);
    char *str = print_object(result);
    check(strcmp("(condition-case e (bar 3) (ohno (bar 9)) (didnt-happen e))", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_macroexpansion_bug()
{
    test_name = "macroexpansion_bug";
    init_interpreter(65536);
    test_eval_string_helper("(defmacro if (p a &optional b) (cond (b `(cond (,p ,a) (t ,b))) (t `(cond (,p ,a) (t nil)))))");
    test_eval_string_helper("(defun %%and (things) (if (eq things nil) nil (let ((x (car things))) `(if ,x ,(%%and (cdr things)) nil))))");
    lisp_object_t result = test_eval_string_helper("(%%and (cons 'a (cons 'b (cons 'c nil))))");
    char *str = print_object(result);
    check(strcmp("(if a (if b (if c nil nil) nil) nil)", str) == 0, "ok");
    free(str);
    str = print_object(macroexpand_all(result));
    check(strcmp("(cond (a (cond (b (cond (c nil) (t nil))) (t nil))) (t nil))", str) == 0, "macroexpand_all ok");
    free(str);
    free_interpreter();
}

static void test_macroexpansion_bug2()
{
    test_name = "macroexpansion_bug2";
    init_interpreter(65536);
    test_eval_string_helper("(defmacro if (p a &optional b) (cond (b `(cond (,p ,a) (t ,b))) (t `(cond (,p ,a) (t nil)))))");
    test_eval_string_helper("(defmacro foo (x) `(if ,x 'ab 'cd))");
    lisp_object_t result = macroexpand_all(parse1_wrapper("(foo 14)"));
    char *str = print_object(result);
    free(str);
    free_interpreter();
}

static void test_lambda_implicit_progn()
{
    test_name = "lambda_implicit_progn";
    test_eval_helper("(funcall (function (lambda (a b) (set 'a 12) (set 'b 14) (cons a b))) 3 4)", "(12 . 14)");
}

static void test_cond_default()
{
    test_name = "cond_default";
    test_eval_helper("(cond ((eq 3 4) 'foo))", "nil");
}

static void test_lisp_heap_cons()
{
    test_name = "lisp_heap_cons";
    init_interpreter(32768);
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
    test_name = "lisp_heap_copy_single_object";
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
    test_name = "lisp_heap_gc_simple";
    init_interpreter(32768);
    char *orig_from_space = interp->heap.from_space;
    char *orig_to_space = interp->heap.to_space;
    check(orig_from_space == interp->heap.heap, "from_space");
    check(orig_to_space == orig_from_space + interp->heap.size_bytes / 2, "to_space");
    free_interpreter();
}

static void test_vector_builtins()
{
    test_name = "vector_builtins";
    test_eval_helper("(let ((x (make-vector 4))) (set-svref x 3 'frob) (set-svref x 2 14) (cons x (cons (svref x 3) (cons (svref x 2)))))", "(#(nil nil 14 frob) frob 14)");
}

static void test_non_symbol_in_function_position()
{
    test_name = "non_symbol_in_function_position";
    test_eval_helper("(condition-case e (2 2) (illegal-function-call e))", "(illegal-function-call . 2)");
}

static void test_type_of()
{
    test_name = "type_of";
    test_eval_helper("(type-of 14)", "integer");
    test_eval_helper("(type-of 'foo)", "symbol");
    test_eval_helper("(type-of (cons 'a 'b))", "cons");
    test_eval_helper("(type-of \"hello\")", "string");
    test_eval_helper("(type-of #(1 2 3))", "vector");
}

static void test_comma_not_inside_backquote()
{
    test_name = "comma_not_inside_backquote";
    test_eval_helper("(condition-case e ,foo (runtime-error e))", "(runtime-error . comma-not-inside-backquote)");
}

static void test_string_equalp()
{
    test_name = "string_equalp";
    test_eval_helper("(string-equal-p \"foo\" \"foo\")", "t");
    test_eval_helper("(string-equal-p \"foo\" \"bar\")", "nil");
}

static void test_length_builtin()
{
    test_name = "length_builtin";
    test_eval_helper("(length '(a b c))", "3");
    test_eval_helper("(length #(1 2 3 4 5))", "5");
    test_eval_helper("(length #( ))", "0");
    test_eval_helper("(length nil)", "0");
}

static void test_parse_empty_vector()
{
    test_name = "parse_empty_vector";
    test_eval_helper("(type-of #())", "vector");
    test_eval_helper("(length #())", "0");
    test_eval_helper("#()", "#()");
}

static void test_quasiquote_bug()
{
    test_name = "quasiquote_bug";
    test_eval_helper("``(foo ,bar)", "`(foo ,bar)");
    test_eval_helper("(let ((bar 14)) ``(foo ,,bar))", "`(foo ,14)");
    test_eval_helper("``(foo ,@bar)", "`(foo ,@bar)");
}

static void test_apply()
{
    test_name = "apply";
    test_eval_helper("(apply 'cons '(a b))", "(a . b)");
}

static void test_parse_function()
{
    test_name = "parse_function";
    init_interpreter(32768);
    lisp_object_t result = parse1_wrapper("#'cons");
    char *str = print_object(result);
    check(strcmp("(function cons)", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_nonexistent_function()
{
    test_name = "nonexistent_function";
    init_interpreter(32768);
    lisp_object_t result = test_eval_string_helper("(condition-case e (function nonexistent) (undefined-function e))");
    char *str = print_object(result);
    check(strcmp("(undefined-function . nonexistent)", str) == 0, "ok");
    free(str);
    free_interpreter();
}

static void test_unquote_splice_bug()
{
    test_name = "unquote_splice_bug";
    init_interpreter(32768);
    lisp_object_t result = test_eval_string_helper("(let ((x '(1 2 3))) `(foo ,@x bar))");
    char *str = print_object(result);
    char *expected = "(foo 1 2 3 bar)";
    check(strcmp(expected, str) == 0, expected);
    free(str);
    free_interpreter();
}

static void test_gensym()
{
    test_name = "gensym";
    init_interpreter(65536);
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
    test_parser_advances_pointer();
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
    test_pairlis();
    test_sym();
    test_evalquote();
    test_eval();
    test_defun();
    test_load();
    test_set();
    test_prog();
    test_rplaca();
    test_rplacd();
    test_rest_args();
    test_plus();
    test_minus();
    test_times();
    test_return_from_prog();
    test_read_token();
    test_numeric_equals();
    test_parse_function_pointer();
    test_print_function_pointer();
    test_call_function_pointer();
    test_integer_bug();
    test_return_outside_prog();
    test_prog_without_return();
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
    test_macroexpand_all_cond();
    test_macroexpand_all_progn();
    test_macroexpand_all_lambda();
    test_macroexpand_all_tagbody();
    test_macroexpand_all_prog();
    test_macroexpand_all_set();
    test_macroexpand_all_let();
    test_macroexpand_all_defun();
    test_macroexpand_all_defmacro();
    test_macroexpand_all_condition_case();
    test_macroexpansion_bug();
    test_macroexpansion_bug2();
    test_lambda_implicit_progn();
    test_cond_default();
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
    if (fail_count)
        printf("%d checks failed\n", fail_count);
    else
        printf("All tests successful\n");
    return fail_count;
}
