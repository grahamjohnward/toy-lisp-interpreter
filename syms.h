#ifndef SYMS_H
#define SYMS_H

struct syms {
    lisp_object_t lambda;
    lisp_object_t quote;
    lisp_object_t built_in_function;
    lisp_object_t progn;
    lisp_object_t tagbody;
    lisp_object_t set;
    lisp_object_t go;
    lisp_object_t amprest;
    lisp_object_t ampbody;
    lisp_object_t ampoptional;
    lisp_object_t condition_case;
    lisp_object_t quasiquote;
    lisp_object_t unquote;
    lisp_object_t unquote_splice;
    lisp_object_t let;
    lisp_object_t integer;
    lisp_object_t symbol;
    lisp_object_t cons;
    lisp_object_t string;
    lisp_object_t vector;
    lisp_object_t macro;
    lisp_object_t function;
    lisp_object_t block;
    lisp_object_t pctblock;
    lisp_object_t return_from;
    lisp_object_t if_;
    lisp_object_t pop;
    lisp_object_t push;
    lisp_object_t swap;
    lisp_object_t call;
    lisp_object_t ret;
    lisp_object_t vm_ins_arity;
    lisp_object_t vm_ins_fp;
    lisp_object_t vm_make_function;
    lisp_object_t vm_make_simple_function;
    lisp_object_t provided_arg_count;
    lisp_object_t abort;
    lisp_object_t jmp;
    lisp_object_t jmp_if_nil;
    lisp_object_t closure;
    lisp_object_t make_env;
    lisp_object_t get;
    lisp_object_t set_tag;
    lisp_object_t tag_jmp;
    lisp_object_t nop;
    lisp_object_t raise;
    lisp_object_t funcall;
    lisp_object_t rest_args;
    lisp_object_t get0;
    lisp_object_t set0;
    lisp_object_t make_env2;
};

#endif
