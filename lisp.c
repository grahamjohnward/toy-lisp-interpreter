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

#define TRACE(obj)                                                                                \
    do {                                                                                          \
        char *str = print_object(obj);                                                            \
        printf("%s:%d %s: %s (%p) = %s\n", __FILE__, __LINE__, __func__, #obj, (void *)obj, str); \
        free(str);                                                                                \
    } while (0);

struct lisp_interpreter *interp;

static int interpreter_initialized;

lisp_object_t *top_of_stack = NULL;

struct symbol {
    object_header_t header;
    lisp_object_t name;
    lisp_object_t value;
    lisp_object_t function;
    lisp_object_t plist;
    uint64_t padding;
};

struct vector {
    object_header_t header;
    size_t len;
    size_t size_bytes;
    uint64_t padding;
};

lisp_object_t istype(lisp_object_t obj, uint64_t type);

static void check_type(lisp_object_t obj, uint64_t type)
{
    static char *type_names[6] = { "unused", "symbol", "cons", "string", "vector", "function pointer" };
    if (istype(obj, type) == NIL) {
        printf("Bad type for %p (not %p)\n", (void *)obj, (void *)type);
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

lisp_object_t functionp(lisp_object_t obj)
{
    return istype(obj, FUNCTION_TYPE);
}

void check_integer(int64_t obj)
{
    if (obj % 16)
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

static lisp_object_t *check_vector_bounds_get_storage(lisp_object_t vector, lisp_object_t index)
{
    check_vector(vector);
    struct vector *v = VectorPtr(vector);
    if (index >= v->len) {
        // This should be an exception
        printf("Index %zu out of bounds for vector (len=%lu)\n", index >> 4, v->len >> 4);
        abort();
    }
    lisp_object_t *storage = (lisp_object_t *)(((char *)v) + sizeof(struct vector));
    return storage;
}

lisp_object_t svref(lisp_object_t vector, lisp_object_t index)
{
    lisp_object_t *storage = check_vector_bounds_get_storage(vector, index);
    return storage[index >> 4];
}

lisp_object_t svref_set(lisp_object_t vector, lisp_object_t index, lisp_object_t newvalue)
{
    lisp_object_t *storage = check_vector_bounds_get_storage(vector, index);
    storage[index >> 4] = newvalue;
    return newvalue;
}

static void gc_if_needed(size_t);

static lisp_object_t allocate_function();

lisp_object_t allocate_vector(lisp_object_t size)
{
    size >>= 4;
    size_t bytes_to_allocate = sizeof(struct vector) + (size + size % 2) * sizeof(lisp_object_t);
    gc_if_needed(bytes_to_allocate);
    struct vector *v = (struct vector *)interp->heap.freeptr;
    interp->heap.freeptr += bytes_to_allocate;
    v->header = VECTOR_TYPE;
    v->len = size << 4;
    v->size_bytes = bytes_to_allocate;
    lisp_object_t *storage = (lisp_object_t *)(((char *)v) + sizeof(struct vector));
    lisp_object_t result = (lisp_object_t)v | VECTOR_TYPE;
    for (int i = 0; i < size; i++)
        storage[i] = NIL;
    return result;
}

static void define_built_in_function(char *symbol_name, void (*function_pointer)(void), int arity)
{
    struct symbol *symptr = SymbolPtr(sym(symbol_name));
    lisp_object_t fp = (((uint64_t)function_pointer) << 4) | FUNCTION_POINTER_TYPE;
    lisp_object_t fn = allocate_function();
    struct lisp_function *fnptr = LispFunctionPtr(fn);
    fnptr->kind = interp->syms.built_in_function;
    fnptr->actual_function = cons(interp->syms.built_in_function, cons(fp, cons(((uint64_t)arity) << 4, NIL)));
    symptr->function = fn;
}

void do_read(int fd, char *dest, size_t len)
{
    int n = read(fd, dest, len);
    if (n < 0) {
        perror("read");
        exit(1);
    }
    if (n != len) {
        fprintf(stderr, "incomplete read\n");
        exit(1);
    }
    return;
}

static void init_symbols()
{
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
    interp->syms.integer = sym("integer");
    interp->syms.symbol = sym("symbol");
    interp->syms.cons = sym("cons");
    interp->syms.string = sym("string");
    interp->syms.vector = sym("vector");
    interp->syms.macro = sym("macro");
    interp->syms.function = sym("function");
    interp->syms.funcall = sym("funcall");
}

lisp_object_t length(lisp_object_t seq);

lisp_object_t greater_than(lisp_object_t o1, lisp_object_t o2)
{
    check_integer(o1);
    check_integer(o2);
    return o1 > o2 ? T : NIL;
}

lisp_object_t less_than(lisp_object_t o1, lisp_object_t o2)
{
    check_integer(o1);
    check_integer(o2);
    return o1 < o2 ? T : NIL;
}

lisp_object_t do_apply(lisp_object_t fn, lisp_object_t args)
{
    /* We apply in a null environment */
    return apply(fn, args, NIL);
}

lisp_object_t quit()
{
    exit(0);
}

lisp_object_t funcall(lisp_object_t fn, lisp_object_t x, lisp_object_t a)
{
    return apply(fn, x, a);
}

lisp_object_t gc();

#define FUNCALL_ARITY -1

static void init_builtins()
{
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
    DEFBUILTIN("make-vector", allocate_vector, 1);
    DEFBUILTIN("svref", svref, 2);
    DEFBUILTIN("set-svref", svref_set, 3);
    DEFBUILTIN("save-image", save_image, 1);
    DEFBUILTIN("type-of", type_of, 1);
    DEFBUILTIN("string-equal-p", string_equalp, 2);
    DEFBUILTIN("length", length, 1);
    DEFBUILTIN("two-arg-greater-than", greater_than, 2);
    DEFBUILTIN("two-arg-less-than", less_than, 2);
    DEFBUILTIN("apply", do_apply, 2);
    DEFBUILTIN("quit", quit, 0);
    DEFBUILTIN("funcall", funcall, FUNCALL_ARITY);
    DEFBUILTIN("gc", gc, 0);
#undef DEFBUILTIN
}

void init_interpeter_from_image(char *image)
{
    int fd = open(image, O_RDONLY);
    if (fd < 0) {
        perror(image);
        exit(1);
    }
    interp = (struct lisp_interpreter *)malloc(sizeof(struct lisp_interpreter));
    assert(sizeof(lisp_object_t) == sizeof(void *));
    interp->environ = NIL;
    interp->prog_return_stack = NULL;
    top_of_stack = NULL;
    do_read(fd, (char *)&interp->symbol_table, sizeof(lisp_object_t));
    do_read(fd, (char *)&interp->heap, sizeof(struct lisp_heap));
    void *rc = mmap(interp->heap.heap, interp->heap.size_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (rc == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    assert(rc == interp->heap.heap);
    do_read(fd, interp->heap.heap, interp->heap.size_bytes);
    init_symbols();
    init_builtins();
    interpreter_initialized = 1;
}

void init_interpreter(size_t heap_size)
{
    interp = (struct lisp_interpreter *)malloc(sizeof(struct lisp_interpreter));
    assert(sizeof(lisp_object_t) == sizeof(void *));
    interp->symbol_table = NIL;
    interp->prog_return_stack = NULL;
    interp->environ = NIL;
    top_of_stack = NULL;
    lisp_heap_init(&interp->heap, heap_size);
    init_symbols();
    init_builtins();
    interpreter_initialized = 1;
}

void lisp_heap_init(struct lisp_heap *heap, size_t bytes)
{
    assert(bytes % 2 == 0);
    assert(bytes % sizeof(lisp_object_t) == 0);
    heap->heap = (char *)mmap((void *)LISP_HEAP_BASE, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (heap->heap == (char *)-1) {
        perror("lisp_heap_init: mmap failed");
        exit(1);
    }
    heap->freeptr = heap->heap;
    heap->size_bytes = bytes;
    heap->from_space = heap->heap;
    heap->to_space = heap->heap + bytes / 2;
}

static void assert_heap_invariants(struct lisp_heap *heap)
{
    assert(heap->freeptr >= heap->heap);
    assert(heap->freeptr <= heap->heap + heap->size_bytes);
    assert(heap->to_space == heap->heap || heap->from_space == heap->heap);
}

void lisp_heap_free(struct lisp_heap *heap)
{
    int rc = munmap(heap->heap, heap->size_bytes);
    if (rc != 0) {
        perror("lisp_heap_free: munmap failed");
        exit(1);
    }
}

static void gc_if_needed(size_t bytes_needed)
{
    struct lisp_heap *heap = &interp->heap;
    assert_heap_invariants(heap);
    if (heap->freeptr + bytes_needed - heap->from_space > heap->size_bytes / 2) {
        gc();
        size_t bytes_in_use_now = heap->freeptr - heap->from_space;
        size_t bytes_free = heap->size_bytes / 2 - bytes_in_use_now;
        if (bytes_free < bytes_needed) {
            printf("Heap exhausted\n");
            exit(1);
        }
    }
}

lisp_object_t cons(lisp_object_t car, lisp_object_t cdr)
{
    struct lisp_heap *heap = &interp->heap;
    gc_if_needed(sizeof(struct cons));
    struct cons *the_cons = (struct cons *)heap->freeptr;
    the_cons->header = CONS_TYPE;
    the_cons->car = car;
    the_cons->cdr = cdr;
    heap->freeptr += sizeof(struct cons);
    return ((lisp_object_t)the_cons) | CONS_TYPE;
}

static lisp_object_t allocate_new_symbol(lisp_object_t name)
{
    check_string(name);
    struct lisp_heap *heap = &interp->heap;
    gc_if_needed(sizeof(struct symbol));
    if (heap->freeptr >= heap->from_space + heap->size_bytes / 2)
        abort();
    struct symbol *s = (struct symbol *)heap->freeptr;
    heap->freeptr += sizeof(struct symbol);
    s->header = SYMBOL_TYPE;
    s->name = name;
    s->value = NIL;
    s->function = NIL;
    s->plist = NIL;
    lisp_object_t symbol = (uint64_t)s | SYMBOL_TYPE;
    interp->symbol_table = cons(symbol, interp->symbol_table);
    return symbol;
}

static lisp_object_t allocate_function()
{
    struct lisp_heap *heap = &interp->heap;
    gc_if_needed(sizeof(struct lisp_function));
    if (heap->freeptr >= heap->from_space + heap->size_bytes / 2)
        abort();
    struct lisp_function *fn = (struct lisp_function *)heap->freeptr;
    heap->freeptr += sizeof(struct lisp_function);
    fn->header = FUNCTION_TYPE;
    fn->kind = NIL;
    fn->actual_function = NIL;
    return (uint64_t)fn | FUNCTION_TYPE;
}

void *get_rbp(int offset)
{
    uint64_t *rbp;
    asm("movq %%rbp, %0"
        : "=r"(rbp));
    for (int i = 0; i < offset; i++)
        rbp = *((uint64_t **)rbp);
    return rbp;
}

static size_t objsize(lisp_object_t obj)
{
    if (consp(obj) != NIL)
        return sizeof(struct cons);
    if (symbolp(obj) != NIL)
        return sizeof(struct symbol);
    if (stringp(obj) != NIL)
        return StringPtr(obj)->allocated_length + sizeof(struct string_header);
    if (vectorp(obj) != NIL)
        return VectorPtr(obj)->size_bytes;
    if (functionp(obj) != NIL)
        return sizeof(struct lisp_function);
    abort();
}

static int object_is_in_from_space(struct lisp_heap *heap, lisp_object_t obj)
{
    assert_heap_invariants(heap);
    uint64_t type = obj & TYPE_MASK;
    char *p = (char *)(obj & PTR_MASK);
    return type > 0 && p >= heap->from_space && p < heap->from_space + heap->size_bytes / 2;
}

static int object_is_in_to_space(struct lisp_heap *heap, lisp_object_t obj)
{
    assert_heap_invariants(heap);
    uint64_t type = obj & TYPE_MASK;
    char *p = (char *)(obj & PTR_MASK);
    return type > 0 && p >= heap->to_space && p < heap->to_space + heap->size_bytes / 2;
}

/* heap is passed for the unit tests */
void gc_copy(struct lisp_heap *heap, lisp_object_t *p)
{
    assert_heap_invariants(heap);
    if (*p == NIL || *p == T)
        return;
    if (consp(*p) == NIL && symbolp(*p) == NIL && stringp(*p) == NIL && vectorp(*p) == NIL && functionp(*p) == NIL)
        return;
    if (symbolp(*p) != NIL) {
        struct symbol *symptr = SymbolPtr(*p);
        if (symptr->name & FORWARDING_POINTER) {
            *p = symptr->name & ~FORWARDING_POINTER;
            return;
        }
    } else if (consp(*p) != NIL) {
        struct cons *consptr = ConsPtr(*p);
        if (consptr->car & FORWARDING_POINTER) {
            *p = consptr->car & ~FORWARDING_POINTER;
            return;
        }
    } else if (stringp(*p) != NIL) {
        struct string_header *string = StringPtr(*p);
        if (string->allocated_length & FORWARDING_POINTER) {
            *p = string->allocated_length & ~FORWARDING_POINTER;
            return;
        }
    } else if (vectorp(*p) != NIL) {
        struct vector *vector = VectorPtr(*p);
        if (vector->len & FORWARDING_POINTER) {
            *p = vector->len & ~FORWARDING_POINTER;
            return;
        }
    } else if (functionp(*p) != NIL) {
        struct lisp_function *fnptr = LispFunctionPtr(*p);
        if (fnptr->kind & FORWARDING_POINTER) {
            *p = fnptr->kind & ~FORWARDING_POINTER;
            return;
        }
    } else {
        abort();
    }

    /* Copy to to-space */
    size_t size = objsize(*p);
    void *ptr = (void *)(*p & PTR_MASK);
    memcpy(heap->freeptr, ptr, size);
    uint64_t type = *p & TYPE_MASK;
    lisp_object_t moved_obj = ((uint64_t)heap->freeptr) | type;
    heap->freeptr += size;
    if (symbolp(*p) != NIL)
        SymbolPtr(*p)->name = moved_obj | FORWARDING_POINTER;
    else if (consp(*p) != NIL)
        ConsPtr(*p)->car = moved_obj | FORWARDING_POINTER;
    else if (stringp(*p) != NIL)
        StringPtr(*p)->allocated_length = moved_obj | FORWARDING_POINTER;
    else if (vectorp(*p) != NIL)
        VectorPtr(*p)->len = moved_obj | FORWARDING_POINTER;
    else if (functionp(*p) != NIL)
        LispFunctionPtr(*p)->kind = moved_obj | FORWARDING_POINTER;
    else
        abort();
    *p = moved_obj;
    assert(object_is_in_to_space(heap, *p));
}

static void gc_check_copied_object(lisp_object_t obj)
{
    if (integerp(obj) != NIL || stringp(obj) != NIL || vectorp(obj) != NIL || function_pointer_p(obj) != NIL || obj == T || obj == NIL)
        return;
    assert(!(obj & FORWARDING_POINTER));
    char *p = (char *)(obj & PTR_MASK);
    assert(p >= interp->heap.from_space && p < interp->heap.from_space + interp->heap.size_bytes / 2);
}

static void *ptr_demangle(void *ptr)
{
    asm("ror $0x11, %rdi");
    asm("xor %fs:0x30, %rdi");
    asm("movq %rdi, -0x8(%rbp)");
    return ptr;
}

static int jmp_buf_entry_is_pointer[] = { 0, 1, 0, 0, 0, 0, 1, 1 };

lisp_object_t gc()
{
    size_t bytes_in_use_before_gc = interp->heap.freeptr - interp->heap.from_space;
    printf("; Garbage collecting ... ");
    struct lisp_heap *heap = &interp->heap;
    heap->freeptr = heap->to_space;
    /* Roots - stack */
    void *rbp = get_rbp(1);
    assert(top_of_stack);
    for (lisp_object_t *p = top_of_stack; p > (lisp_object_t *)rbp; p--)
        if (object_is_in_from_space(heap, *p))
            gc_copy(&interp->heap, p);
    /* Roots - return contexts */
    for (struct return_context *ctxt = interp->prog_return_stack; ctxt; ctxt = ctxt->next) {
        gc_copy(heap, &ctxt->return_value);
        gc_copy(heap, &ctxt->type);
        for (int i = 0; i < ctxt->tagbody_forms_len; i++)
            gc_copy(heap, &ctxt->tagbody_forms[i]);
        for (int i = 0; i < 8; i++) {
            if (jmp_buf_entry_is_pointer[i]) {
#ifdef __GLIBC__
                void *jmpbuf_entry = (void *)(ctxt->buf->__jmpbuf[i]);
                lisp_object_t *p = ptr_demangle(jmpbuf_entry);
#else
                /* musl libc, which does not define a preprocessor symbol */
                lisp_object_t *p = (lisp_object_t *)(ctxt->buf->__jb[i]);
#endif
                if (object_is_in_from_space(heap, *p))
                    gc_copy(&interp->heap, p);
            }
        }
    }
    /* Roots - symbol table */
    gc_copy(heap, &interp->symbol_table);
#define GC_COPY_SYMBOL(S) gc_copy(heap, &interp->syms.S)
    GC_COPY_SYMBOL(lambda);
    GC_COPY_SYMBOL(quote);
    GC_COPY_SYMBOL(cond);
    GC_COPY_SYMBOL(defun);
    GC_COPY_SYMBOL(built_in_function);
    GC_COPY_SYMBOL(prog);
    GC_COPY_SYMBOL(progn);
    GC_COPY_SYMBOL(tagbody);
    GC_COPY_SYMBOL(set);
    GC_COPY_SYMBOL(go);
    GC_COPY_SYMBOL(return_);
    GC_COPY_SYMBOL(amprest);
    GC_COPY_SYMBOL(ampbody);
    GC_COPY_SYMBOL(ampoptional);
    GC_COPY_SYMBOL(condition_case);
    GC_COPY_SYMBOL(defmacro);
    GC_COPY_SYMBOL(quasiquote);
    GC_COPY_SYMBOL(unquote);
    GC_COPY_SYMBOL(unquote_splice);
    GC_COPY_SYMBOL(let);
    GC_COPY_SYMBOL(integer);
    GC_COPY_SYMBOL(symbol);
    GC_COPY_SYMBOL(cons);
    GC_COPY_SYMBOL(string);
    GC_COPY_SYMBOL(vector);
    GC_COPY_SYMBOL(macro);
    GC_COPY_SYMBOL(function);
    GC_COPY_SYMBOL(funcall);
#undef GC_COPY_SYMBOL
    /* Update pointers inside to-space objects */
    char *scanptr;
    for (scanptr = heap->to_space; scanptr < heap->freeptr;) {
        object_header_t *headerptr = (object_header_t *)scanptr;
        if (*headerptr == CONS_TYPE) {
            struct cons *consptr = (struct cons *)scanptr;
            gc_copy(heap, &consptr->car);
            gc_copy(heap, &consptr->cdr);
            scanptr += sizeof(struct cons);
        } else if (*headerptr == SYMBOL_TYPE) {
            struct symbol *symptr = (struct symbol *)scanptr;
            gc_copy(heap, &symptr->name);
            gc_copy(heap, &symptr->value);
            gc_copy(heap, &symptr->function);
            gc_copy(heap, &symptr->plist);
            scanptr += sizeof(struct symbol);
        } else if (*headerptr == STRING_TYPE) {
            struct string_header *strptr = (struct string_header *)scanptr;
            scanptr += strptr->allocated_length + sizeof(struct string_header);
        } else if (*headerptr == VECTOR_TYPE) {
            struct vector *v = (struct vector *)scanptr;
            lisp_object_t *storage = (lisp_object_t *)(scanptr + sizeof(struct vector));
            for (int i = 0; i < v->len >> 4; i++)
                gc_copy(heap, storage + i);
            scanptr += v->size_bytes;
        } else if (*headerptr == FUNCTION_TYPE) {
            struct lisp_function *fnptr = (struct lisp_function *)scanptr;
            gc_copy(heap, &fnptr->kind);
            gc_copy(heap, &fnptr->actual_function);
            scanptr += sizeof(struct lisp_function);
        } else {
            abort();
        }
    }
    assert(scanptr == heap->freeptr);
    /* Swap spaces */
    char *tmp = heap->from_space;
    heap->from_space = heap->to_space;
    heap->to_space = tmp;
    /* Make assertions about copied objects */
    for (char *p = heap->from_space; p < heap->freeptr;) {
        object_header_t *headerptr = (object_header_t *)p;
        if (*headerptr == CONS_TYPE) {
            struct cons *c = (struct cons *)p;
            if (c->car != NIL && consp(c->car) != NIL)
                gc_check_copied_object(c->car);
            if (c->cdr != NIL && consp(c->cdr) != NIL)
                gc_check_copied_object(c->cdr);
            p += sizeof(struct cons);
        } else if (*headerptr == SYMBOL_TYPE) {
            struct symbol *sym = (struct symbol *)p;
            gc_check_copied_object(sym->name);
            gc_check_copied_object(sym->function);
            gc_check_copied_object(sym->value);
            gc_check_copied_object(sym->plist);
            p += sizeof(struct symbol);
        } else if (*headerptr == STRING_TYPE) {
            struct string_header *strptr = (struct string_header *)p;
            p += strptr->allocated_length + sizeof(struct string_header);
        } else if (*headerptr == FUNCTION_TYPE) {
            struct lisp_function *fnptr = (struct lisp_function *)p;
            gc_check_copied_object(fnptr->kind);
            gc_check_copied_object(fnptr->actual_function);
            p += sizeof(struct lisp_function);
        } else {
            struct vector *v = (struct vector *)p;
            lisp_object_t *storage = (lisp_object_t *)(p + sizeof(struct vector));
            for (int i = 0; i < v->len >> 4; i++)
                gc_check_copied_object(storage[i]);
            p += v->size_bytes;
        }
    }
    /* Say how much memory was freed */
    size_t bytes_in_use_now = interp->heap.freeptr - interp->heap.from_space;
    printf("%lu bytes freed\n", bytes_in_use_before_gc - bytes_in_use_now);
    return T;
}

void free_interpreter()
{
    if (interpreter_initialized) {
        lisp_heap_free(&interp->heap);
        free(interp);
        interpreter_initialized = 0;
    }
}

/* len here includes the terminating null byte of str */
lisp_object_t allocate_string(size_t len, char *str)
{
    assert(!str[len - 1]);
    /* I want to make this a multiple of 16
     * but not sure that is actually needed
     */
    size_t bytes_to_allocate_for_actual_string = ((len / 16) + 1) * 16;
    size_t total_bytes_to_allocate = sizeof(struct string_header) + bytes_to_allocate_for_actual_string;
    gc_if_needed(total_bytes_to_allocate);
    struct string_header *new_string = (struct string_header *)interp->heap.freeptr;
    new_string->header = STRING_TYPE;
    new_string->allocated_length = bytes_to_allocate_for_actual_string;
    new_string->string_length = len;
    char *new_string_storage = ((char *)new_string) + sizeof(struct string_header);
    interp->heap.freeptr += total_bytes_to_allocate;
    strncpy(new_string_storage, str, len);
    return (lisp_object_t)new_string | STRING_TYPE;
}

void get_string_parts(lisp_object_t string, size_t *lenptr, char **strptr)
{
    check_string(string);
    struct string_header *header = StringPtr(string);
    *lenptr = header->string_length - 1;
    *strptr = ((char *)header) + sizeof(struct string_header);
}

lisp_object_t string_equalp(lisp_object_t s1, lisp_object_t s2)
{
    check_string(s1);
    check_string(s2);
    if (eq(s1, s2) != NIL) {
        return T;
    } else {
        size_t l1, l2;
        char *str1, *str2;
        get_string_parts(s1, &l1, &str1);
        get_string_parts(s2, &l2, &str2);
        if (l1 != l2)
            return NIL;
        return strncmp(str1, str2, l1) == 0 ? T : NIL;
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
        return allocate_new_symbol(name);
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
        lisp_object_t tmp = parse1(ts);
        rplacd(new_cons, tmp);
    }
    if (tspeek(ts) == ')') {
        text_stream_advance(ts);
        return new_cons;
    } else {
        /* This temporary is needed to get the value on the stack for the GC to see */
        lisp_object_t tmp = parse_cons(ts);
        rplacd(new_cons, tmp);
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
        return v->len >> 4;
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

lisp_object_t length(lisp_object_t seq)
{
    return length_c(seq) << 4;
}

lisp_object_t parse_vector(struct text_stream *ts)
{
    assert(tspeek(ts) == '(');
    text_stream_advance(ts);
    skip_whitespace(ts);
    if (tspeek(ts) == ')') {
        /* Special case empty vector */
        text_stream_advance(ts);
        return allocate_vector(0);
    }
    lisp_object_t list = parse_cons(ts);
    int len = length_c(list);
    lisp_object_t vector = allocate_vector(len << 4);
    /* Copy the list into a vector */
    int i;
    lisp_object_t c;
    for (i = 0, c = list; i < len; i++, c = cdr(c))
        svref_set(vector, i << 4, car(c));
    return vector;
}

lisp_object_t parse_dispatch(struct text_stream *ts)
{
    assert(tspeek(ts) == '#');
    text_stream_advance(ts);
    if (!tspeek(ts))
        abort();
    switch (tspeek(ts)) {
    case '(':
        return parse_vector(ts);
    case '\'':
        text_stream_advance(ts);
        return cons(interp->syms.function, cons(parse1(ts), NIL));
    default:
        abort();
    }
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
            return base == 16 ? ((val << 4) | FUNCTION_POINTER_TYPE) : (val << 4);
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
        int64_t value = ((int64_t)obj) / 16;
        int length = snprintf(NULL, 0, "%ld", value);
        char *str = alloca(length + 1);
        snprintf(str, length + 1, "%ld", value);
        string_buffer_append(sb, str);
    } else if (obj == NIL) {
        string_buffer_append(sb, "nil");
    } else if (obj == T) {
        string_buffer_append(sb, "t");
    } else if (consp(obj) != NIL) {
        if (car(obj) == interp->syms.quote) {
            string_buffer_append(sb, "'");
            print_object_to_buffer(cadr(obj), sb);
        } else if (car(obj) == interp->syms.quasiquote) {
            string_buffer_append(sb, "`");
            print_object_to_buffer(cadr(obj), sb);
        } else if (car(obj) == interp->syms.unquote) {
            string_buffer_append(sb, ",");
            print_object_to_buffer(cadr(obj), sb);
        } else if (car(obj) == interp->syms.unquote_splice) {
            string_buffer_append(sb, ",@");
            print_object_to_buffer(cadr(obj), sb);
        } else {
            string_buffer_append(sb, "(");
            print_cons_to_buffer(obj, sb);
            string_buffer_append(sb, ")");
        }
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
            print_object_to_buffer(svref(obj, i << 4), sb);
            string_buffer_append(sb, " ");
        }
        if (len > 0)
            print_object_to_buffer(svref(obj, (len - 1) << 4), sb);
        string_buffer_append(sb, ")");
    } else if (function_pointer_p(obj) != NIL) {
        char *buf = alloca(32);
        sprintf(buf, "%p", (FunctionPtr(obj)));
        string_buffer_append(sb, buf);
    } else if (functionp(obj) != NIL) {
        string_buffer_append(sb, "#<function>");
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
    lisp_object_t x = car(obj);
    return car(x);
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
    ctxt->tagbody_forms_len = 0;
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
        printf("Unhandled exception: %s\n", message);
        free(message);
        abort();
    }
    interp->prog_return_stack->return_value = value;
    longjmp(interp->prog_return_stack->buf, 1);
    return NIL; /* we never actually return */
}

static lisp_object_t apply_lambda(lisp_object_t fn, lisp_object_t x, lisp_object_t a)
{
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
}

static lisp_object_t apply_built_in_function(lisp_object_t fn, lisp_object_t x, lisp_object_t a)
{
    check_function_pointer(cadr(fn));
    void (*fp)() = FunctionPtr(cadr(fn));
    int arity = ((int64_t)caddr(fn)) >> 4;
    switch (arity) {
    case 0:
        return ((lisp_object_t(*)())fp)();
    case 1:
        return ((lisp_object_t(*)(lisp_object_t))fp)(car(x));
    case 2:
        return ((lisp_object_t(*)(lisp_object_t, lisp_object_t))fp)(car(x), cadr(x));
    case 3:
        return ((lisp_object_t(*)(lisp_object_t, lisp_object_t, lisp_object_t))fp)(car(x), cadr(x), caddr(x));
    case FUNCALL_ARITY:
        return ((lisp_object_t(*)(lisp_object_t, lisp_object_t, lisp_object_t))fp)(car(x), cdr(x), a);
    default:
        abort();
    }
}

lisp_object_t apply(lisp_object_t fn, lisp_object_t x, lisp_object_t a)
{
    if (atom(fn) != NIL) {
        if (fn == NIL) {
            return raise(sym("illegal-function-call"), fn);
        }
        if (symbolp(fn) != NIL) {
            // Check whether it is s symbol with function binding
            struct symbol *symptr = SymbolPtr(fn);
            if (symptr->function != NIL) {
                fn = symptr->function;
            } else {
                return raise(sym("illegal-function-call"), fn);
            }
        } else if (functionp(fn) == NIL) {
            return raise(sym("illegal-function-call"), fn);
        }
        struct lisp_function *fnptr = LispFunctionPtr(fn);
        if (fnptr->actual_function != NIL) {
            if (fnptr->kind == interp->syms.lambda)
                return apply_lambda(fnptr->actual_function, x, a);
            else if (fnptr->kind == interp->syms.built_in_function)
                return apply_built_in_function(fnptr->actual_function, x, a);
            else
                abort();
        } else {
            abort();
        }
    } else {
        return raise(sym("illegal-function-call"), fn);
    }
}

lisp_object_t evcon(lisp_object_t c, lisp_object_t a)
{
    if (c == NIL)
        return NIL;
    else if (eval(caar(c), a) != NIL)
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
    lisp_object_t fn_new = allocate_function();
    struct lisp_function *fnptr = LispFunctionPtr(fn_new);
    fnptr->kind = interp->syms.lambda;
    fnptr->actual_function = fn;
    struct symbol *sym = SymbolPtr(fname);
    sym->function = fn_new;
    return fname;
}

lisp_object_t evaldefmacro(lisp_object_t e, lisp_object_t a)
{
    lisp_object_t fname = evaldefun(e, a);
    putprop(fname, interp->syms.macro, T);
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
    interp->prog_return_stack->tagbody_forms_len = i;
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

lisp_object_t eval_function(lisp_object_t function, lisp_object_t a)
{
    if (symbolp(function) != NIL) {
        struct symbol *symptr = SymbolPtr(function);
        if (symptr->function != NIL)
            return symptr->function;
        else
            return raise(sym("undefined-function"), function);
    } else {
        lisp_object_t fn = allocate_function();
        struct lisp_function *fnptr = LispFunctionPtr(fn);
        fnptr->kind = interp->syms.lambda;
        fnptr->actual_function = function;
        return fn;
    }
}

lisp_object_t eval_quasiquote(lisp_object_t e, lisp_object_t a, int depth)
{
    if (e == NIL) {
        return NIL;
    } else if (atom(e) != NIL) {
        return e;
    } else if (consp(e) != NIL) {
        if (eq(car(e), interp->syms.quasiquote) != NIL) {
            return cons(interp->syms.quasiquote, cons(eval_quasiquote(cadr(e), a, depth + 1), NIL));
        } else if (eq(car(e), interp->syms.unquote) != NIL) {
            if (depth == 0)
                return eval(cadr(e), a);
            else
                return cons(interp->syms.unquote, cons(eval_quasiquote(cadr(e), a, depth - 1), NIL));
        } else if (eq(car(e), interp->syms.unquote_splice) != NIL) {
            abort();
        } else if (consp(car(e)) != NIL && eq(car(car(e)), interp->syms.unquote_splice) != NIL) {
            if (depth == 0)
                return eval(cadr(car(e)), a);
            else
                return cons(cons(interp->syms.unquote_splice, cons(eval_quasiquote(cadar(e), a, depth - 1), NIL)), NIL);
        } else {
            return cons(eval_quasiquote(car(e), a, depth), eval_quasiquote(cdr(e), a, depth));
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
    if (consp(e) != NIL && symbolp(car(e)) != NIL && getprop(car(e), interp->syms.macro) != NIL) {
        return cons(eval(cons(car(e), quote_list(cdr(e))), a), T);
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

static lisp_object_t macroexpand_all_quasiquote(lisp_object_t e)
{
    if (atom(e) != NIL)
        return e;
    else if (car(e) == interp->syms.unquote || car(e) == interp->syms.unquote_splice)
        return cons(car(e), cons(macroexpand_all(cadr(e)), NIL));
    else
        return cons(car(e), cons(macroexpand_all_quasiquote(cadr(e)), NIL));
}

lisp_object_t macroexpand_all(lisp_object_t e)
{
    e = macroexpand(e, NIL);
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
        } else if (sym == interp->syms.quasiquote) {
            return cons(sym, macroexpand_all_quasiquote(cdr(e)));
        } else if (sym == interp->syms.function) {
            return e;
        } else {
            // This covers function calls, but also special forms that look like them,
            // e.g. `go`, `set`.
            return cons(car(e), macroexpand_all_list(cdr(e)));
        }
    } else {
        return macroexpand_all_list(e);
    }
}

lisp_object_t eval_function_call(lisp_object_t e, lisp_object_t a)
{
    lisp_object_t fn = car(e);
    if (symbolp(fn) != NIL) {
        struct symbol *s = SymbolPtr(fn);
        if (s->function != NIL) {
            return apply(eval_function(car(e), a), evlis(cdr(e), a), a);
        } else {
            return raise(sym("undefined-function"), fn);
        }
    } else {
        return raise(sym("illegal-function-call"), fn);
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
            return eval_quasiquote(cadr(e), a, 0);
        } else if (eq(car(e), interp->syms.unquote) != NIL) {
            return raise(sym("runtime-error"), sym("comma-not-inside-backquote"));
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
        } else if (eq(car(e), interp->syms.function) != NIL) {
            return eval_function(cadr(e), a);
        } else {
            return eval_function_call(e, a);
        }
    } else {
        return raise(sym("illegal-function-call"), car(e));
    }
}

lisp_object_t evalquote(lisp_object_t fn, lisp_object_t x)
{
    return apply(eval_function(fn, NIL), x, NIL);
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
        perror(str);
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
    text_stream_init_fd(&ts, 0);
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

lisp_object_t type_of(lisp_object_t obj)
{
    switch (obj & TYPE_MASK) {
    case SYMBOL_TYPE:
        return interp->syms.symbol;
    case CONS_TYPE:
        return interp->syms.cons;
    case STRING_TYPE:
        return interp->syms.string;
    case VECTOR_TYPE:
        return interp->syms.vector;
    default:
        if (integerp(obj))
            return interp->syms.integer;
        else
            abort();
    }
}

static void do_write(int fd, char *p, size_t len)
{
    size_t bytes_written = 0;
    do {
        int n = write(fd, p, len - bytes_written);
        if (n < 0) {
            perror("write");
            abort();
        }
        p += n;
        bytes_written += n;
    } while (bytes_written < len);
}

lisp_object_t save_image(lisp_object_t name)
{
    size_t len;
    char *str;
    get_string_parts(name, &len, &str);
    int fd = open(str, O_CREAT | O_WRONLY | O_APPEND | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        perror("save_image: open");
        exit(1);
    }
    gc();
    do_write(fd, (char *)&interp->symbol_table, sizeof(lisp_object_t));
    do_write(fd, (char *)&interp->heap, sizeof(struct lisp_heap));
    do_write(fd, interp->heap.heap, interp->heap.size_bytes);
    close(fd);
    exit(0);
}
