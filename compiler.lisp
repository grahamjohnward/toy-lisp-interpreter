(defmacro trace (v &optional message)
  (if message
      `(print (list :trace ,message ',v ,v))
      `(print (list :trace ',v ,v))))

;; not used
(defun list-to-vector (list)
  (let ((length (length list)))
    (let ((vector (make-vector length))
	  (i 0))
      (dolist (obj list)
	(set-svref vector i obj)
	(setq i (+ 1 i)))
      vector)))

(defun build-label-alist (list)
  (let ((i 0)
	(label-alist nil))
    (dolist (obj list)
      (if (and (consp obj) (eq (car obj) 'label))
	  (let ((label (cadr obj)))
	    (when (not (null (assoc label label-alist)))
	      (raise 'duplicate-label))
	    (setq label-alist (cons (cons label i) label-alist)))
	  (setq i (+ 1 i))))
    (cons label-alist i)))

(defun assemble (list)
  (let ((label-info (build-label-alist list)))
    (let ((label-alist (car label-info))
	  (vector (make-vector (cdr label-info)))
	  (i 0))
      (dolist (obj list)
        (when (functionp obj)
          (%asm abort))
	(if (consp obj)
	    (let ((foo (car obj)))
	      (cond ((eq foo 'target)
		     (progn
		       (set-svref vector i (cdr (assoc (cadr obj) label-alist)))
                       (when (functionp (cdr (assoc (cadr obj) label-alist)))
                         (%asm abort))
		       (incf i)))
		    ((eq foo 'literally)
		     (progn
		       (set-svref vector i (cadr obj))
		       (incf i)))))
	    (progn
	      (set-svref vector i obj)
	      (incf i))))
      vector)))

(defun lexical-context-block-alist (ctxt)
  (svref ctxt 0))

(defun lexical-context-next-block-number (ctxt)
  (svref ctxt 1))

(defun lexical-context-bindings (ctxt)
  (svref ctxt 2))

(defun lexical-context-binding-count (ctxt)
  (svref ctxt 3))

(defun lexical-context-set-block-alist (ctxt block-alist)
  (set-svref ctxt 0 block-alist))

(defun lexical-context-push-block (ctxt block-name block-id)
  (lexical-context-set-block-alist ctxt (cons `(,block-name . ,block-id)
					      (lexical-context-block-alist ctxt))))

(defun lexical-context-lookup-block (ctxt block-name)
  (let ((block-alist (lexical-context-block-alist ctxt)))
    (let ((pair (assoc block-name block-alist)))
      (when (null pair)
	(raise 'bad-block block-name))
      (cdr pair))))

(defun lexical-context-pop-block (ctxt)
  (lexical-context-set-block-alist ctxt
				   (cdr (lexical-context-block-alist ctxt))))

(defun lexical-context-set-next-block-number (ctxt next-block-number)
  (set-svref ctxt 1 next-block-number))

(defun lexical-context-set-bindings (ctxt bindings)
  (set-svref ctxt 2 bindings))

(defun lexical-context-set-binding-count (ctxt count)
  (set-svref ctxt 3 count))

(defun lexical-context-push-tag-table (ctxt tag-table)
  (set-svref ctxt 4 (cons tag-table (svref ctxt 4))))

(defun lexical-context-tag-lookup (ctxt tag)
  (let ((tag-tables (svref ctxt 4)))
    (dolist (tag-table tag-tables)
      (let ((pair (assoc tag tag-table)))
	(when pair
	  (return-from lexical-context-tag-lookup (cdr pair))))))
  nil)

(defun lexical-context-pop-tag-table (ctxt)
  (set-svref ctxt 4 (cdr (svref ctxt 4))))

(defun make-lexical-context ()
  (let ((ctxt (make-vector 5)))
    (lexical-context-set-block-alist ctxt nil)
    (lexical-context-set-next-block-number ctxt 0)
    (lexical-context-set-bindings ctxt nil)
    (lexical-context-set-binding-count ctxt 0)
    ctxt))

(defun make-lexical-scope-bindings (bindings)
  (cons 0 bindings))

(defun lexical-context-get-actual-bindings (bindings)
  (cdr bindings))

(defun lexical-context-wowza (ctxt)
  (caar (lexical-context-bindings ctxt)))

(defun lexical-context-enter-scope (ctxt bindings)
  (lexical-context-set-bindings ctxt
                                (cons (make-lexical-scope-bindings bindings)
				      (lexical-context-bindings ctxt)))
  (lexical-context-set-binding-count ctxt
				     (+ (lexical-context-binding-count ctxt)
					(length bindings))))

(defun lexical-context-leave-scope (ctxt)
  (let ((bindings (lexical-context-bindings ctxt)))
    (lexical-context-set-binding-count ctxt
				       (- (lexical-context-binding-count ctxt)
					  (length (car bindings))))
    (lexical-context-set-bindings ctxt (cdr (lexical-context-bindings ctxt)))))

(defun lexical-context-lookup-internal (bindings symbol n)
  (when (null bindings)
    (return-from lexical-context-lookup-internal nil))
  (let ((m 1)                           ;Slot within the scope
        (bindings-wrapper (car bindings)))
    (let ((bindings-one-scope
           (lexical-context-get-actual-bindings bindings-wrapper)))
      (while (not (null bindings-one-scope))
        (when (eq (car bindings-one-scope) symbol)
          (when (> n (car bindings-wrapper))
            (rplaca bindings-wrapper n))
	  (return-from lexical-context-lookup-internal (list n m)))
	(incf m)
	(setq bindings-one-scope (cdr bindings-one-scope))))
    ;; Need to look at the next set of bindings out
    (when (> (+ 1 n) (car bindings-wrapper))
      (rplaca bindings-wrapper n))
    (lexical-context-lookup-internal (cdr bindings) symbol (+ n 1))))

(defun lexical-context-lookup (ctxt symbol)
  (lexical-context-lookup-internal (lexical-context-bindings ctxt) symbol 0))

;; Having these three symbols in the code wreaks havoc with quasiquote
;; expansion:
(defparameter _quasiquote (make-symbol "quasiquote"))
(defparameter _unquote (make-symbol "unquote"))
(defparameter _unquote-splice (make-symbol "unquote-splice"))

(defparameter *depth* 0)

(defparameter *indent-offset* 2)

(defmacro trace1 (v &optional message)
  `(let ((value ,v))
     (%trace1 ',v value ,message)))

(defun %trace1 (v value &optional message)
  (dotimes (i *depth*)
    (princ " "))
  (if message
      (print (list :trace message v value))
      (print (list :trace v value))))

(defun convert-quasiquote (expr depth)
  (setq *depth* (+ *depth* *indent-offset*))
  (let ((result (%convert-quasiquote expr depth)))
    (setq *depth* (- *depth* *indent-offset*))
    result))

(defun %convert-quasiquote (expr depth)
  (cond ((= depth 0)
	 ;; Regular, non-quasiquoted code
	 (cond ((vectorp expr)
		(let ((new-vector (make-vector (length expr))))
		  (dotimes (i (length expr))
		    (set-svref new-vector i
			       (convert-quasiquote (svref expr i) depth)))
		  new-vector))
	       ((consp expr)
		(cond ((eq (car expr) _unquote)
		       (progn
			 (print (list :bad expr))
			 (assert (eq :bog :boo))))	;error
		      ((eq (car expr) _quasiquote)
		       (convert-quasiquote (cadr expr) (+ 1 depth)))
		      (t (cons (convert-quasiquote (car expr) depth)
			       (convert-quasiquote (cdr expr) depth)))))
	       ((symbolp expr)
		expr)
	       (t expr)))
	((and (consp expr)
	      (consp (car expr))
	      (eq (caar expr) 'unquote-splice))
	 ;; ((unquote-splice <unquoted>) <rest>)
	 (cond ((= depth 1)
		(let ((unquoted (car (cdar expr)))
		      (rest (cdr expr)))
		  (let ((var (gensym)))
		    `(let ((,var ,(convert-quasiquote unquoted 0)))
		       (append ,var ,(convert-quasiquote rest depth))))))
	       (t
		`((,_unquote-splice
		   ,(convert-quasiquote (cdar expr) (- depth 1)))))))
	((> depth 0)
	 (cond ((vectorp expr)
		`(apply #'vector ,@(convert-quasiquote (cdr expr) depth)))
	       ((consp expr)
                (cond ((eq (car expr) _unquote)
		       (convert-quasiquote (cadr expr) (- depth 1)))
		      ((eq (car expr) 'unquote-splice)
		       (convert-quasiquote (cadr expr) (- depth 1)))
		      ((eq (car expr) _quasiquote)
		       `(cons _quasiquote
			      ,(convert-quasiquote (cdr expr) (+ depth 1))))
		      (t
		       `(cons ,(convert-quasiquote (car expr) depth)
			      ,(convert-quasiquote (cdr expr) depth)))))
	       ((symbolp expr)
		`(quote ,expr))
	       (t expr)))
	(t (assert nil))))

(defun compile4-list (forms ctxt)
  (if (null forms)
      nil
      (if (null (cdr forms))
	  (append (compile4 (car forms) ctxt) (compile4-list (cdr forms) ctxt))
	  (append (compile4 (car forms) ctxt) '(pop) (compile4-list (cdr forms) ctxt)))))
  
(defun compile4-progn (expr ctxt)
  (assert (eq (car expr) 'progn))
  (compile4-list (cdr expr) ctxt))

(defun parse-arglist (arglist)
  (let ((result (make-vector 3)))
    (let ((i 0) has-rest)
      (tagbody
	 (dolist (arg arglist)
	   (incf i)
	   (when (or (eq arg '&rest) (eq arg '&body))
	     (setq has-rest t)
	     (go done)))
       done
	 (set-svref result 0 has-rest)
	 (set-svref result 1 i)
	 (return-from parse-arglist result)))))


(defun instruction-arity (ins)
  (cond ((find ins '(call set-dso pop nop ret raise swap abort)) 0)
        ((find ins '(push get1 set1 jmp-if-nil jmp tag-jmp rest-args setup-env)) 1)
        ((find ins '(get set set-tag)) 2)
        ((consp ins) 0)
        (t (assert nil))))

(defun wow-amazing (code)
  (let ((result (%wow-amazing code)))
;    (trace result)
    result))

(defun %wow-amazing (code)
  (if (null code) nil
      (let ((ins (car code)))
        (cond ((and (eq ins 'get) (eq (cadr code) 0))
               `(get0 ,(caddr code) ,@(wow-amazing (cdr (cddr code)))))
              ((and (eq ins 'set) (eq (cadr code) 0))
               `(set0 ,(caddr code) ,@(wow-amazing (cdr (cddr code)))))
              (t
               (let ((arity (instruction-arity ins)))
                 (cond ((eq arity 0)
                        `(,ins ,@(wow-amazing (cdr code))))
                       ((eq arity 1)
                        `(,ins ,(cadr code) ,@(wow-amazing (cddr code))))
                       ((eq arity 2)
                        `(,ins ,(cadr code) ,(caddr code) ,@(wow-amazing (cdr (cddr code)))))
                       (t (assert nil)))))))))

;; (append a b) <=> `(,@a ,@b)
(defun compile4-lambda (expr ctxt)
  (assert (eq (car expr) 'lambda))
  (let ((arglist (cadr expr))
	(body (cddr expr)))
    (lexical-context-enter-scope ctxt
				 (remove-if-not #'(lambda (x)
						    (not (or
                                                          (eq x '&optional)
							  (eq x '&rest)
							  (eq x '&body))))
						arglist))
    (let ((compiled-body `(,@(compile4 `(progn ,@body) ctxt) ret)))
      (let ((can-stack-allocate (= (lexical-context-wowza ctxt) 0)))
        (when can-stack-allocate
          (setq compiled-body (wow-amazing compiled-body)))
        (lexical-context-leave-scope ctxt)
        (let ((argcount (length arglist))
	      (arg-info (parse-arglist arglist)))
          (let ((arity (svref arg-info 1))
                (has-rest-args (svref arg-info 0)))
            (let ((preamble (if can-stack-allocate
                                 `(make-env2 ,arity)
                                 `(make-env ,arity))))
              (if has-rest-args
                  (setq compiled-body `(rest-args ,arity ,@preamble
                                                  ,@compiled-body))
                  (setq compiled-body `(,@preamble ,@compiled-body)))))
	  (let ((code (assemble compiled-body)))
	    (let ((result `(push ,arg-info push ,code
                                 push 2 push %vm-make-function call)))
	      result)))))))

(defun compile4-if (expr ctxt)
  (assert (eq (car expr) 'if))
  (let ((test-form (cadr expr))
	(then-form (car (cddr expr)))
	(else-form (cadr (cddr expr))))
    (let ((label1 (gensym))
	  (label2 (gensym)))
      `(,@(compile4 test-form ctxt)
	  jmp-if-nil (target ,label1)
	  ,@(compile4 then-form ctxt)
	  jmp (target ,label2)
	  (label ,label1)
	  ,@(compile4 else-form ctxt)
	  (label ,label2)
	  nop))))

(defun standardize-let-bindings (bindings)
  (mapcar #'(lambda (binding)
	      (if (consp binding)
		  binding
		  (list binding nil)))
	  bindings))

(defun transform-let (let-form)
  (assert (eq (car let-form) 'let))
  (let ((bindings (standardize-let-bindings (cadr let-form)))
	(body (cddr let-form)))
    (let ((arglist (mapcar #'car bindings))
	  (values (mapcar #'cadr bindings)))
      `(funcall #'(lambda ,arglist ,@body) ,@values))))

(defun compile4-let (expr ctxt)
  (assert (eq (car expr) 'let))
  (compile4 (transform-let expr) ctxt))

;; Because we don't have closures in the interpreted language
(defun mapcar-with-context (function list context)
  (if (null list)
      nil
      (cons (funcall function (car list) context)
	    (mapcar-with-context function (cdr list) context))))

(defun compile4-tagbody (expr ctxt)
  (assert (eq (car expr) 'tagbody))
  (let (tag-alist)
    (dolist (form (cdr expr))
      (when (symbolp form)		;form is tag
	(let ((label (gensym)))
	  (setq tag-alist (cons (cons form label) tag-alist)))))
    (lexical-context-push-tag-table ctxt tag-alist)
    (prog1
	(append
	 (apply #'append (mapcar #'(lambda (foo)
				     `(set-tag ,(cdr foo) (target ,(cdr foo))))
				 tag-alist))
	 (apply #'append
		(mapcar-with-context
		 #'(lambda (form ctxt)
		     (if (symbolp form)
			 `((label
			    ,(lexical-context-tag-lookup ctxt form)))
			 (append (compile4 form ctxt) '(pop))))
		 (cdr expr) ctxt))
	 '(push nil))
      (lexical-context-pop-tag-table ctxt))))

(defun compile4-go (expr ctxt)
  (assert (eq (car expr) 'go))
  (let ((tag (cadr expr)))
    (let ((target (lexical-context-tag-lookup ctxt tag)))
      `(tag-jmp ,target))))

(defun compile4-asm (expr ctxt)
  (assert (eq (car expr) '%asm))
  (cdr expr))

(defun compile4-set (expr ctxt)
  (assert (eq (car expr) 'set))
  (let ((var (cadr (cadr expr)))
	(val (caddr expr)))
    (let ((lookup-result (lexical-context-lookup ctxt var)))
      (if lookup-result
	  `(,@(compile4 val ctxt) set ,@lookup-result)
	  `(push ,var ,@(compile4 val ctxt) push 2 push set-symbol-value call)))))

(defun compile4-block (expr ctxt)
  (assert (eq (car expr) 'block))
  (let ((block-name (cadr expr))
	(block-id (gensym)))
    (lexical-context-push-block ctxt block-name block-id)
    (prog1
	`(set-tag ,block-id (target ,block-id)
		  ,@(compile4 `(progn ,@(cddr expr)) ctxt)
		  (label ,block-id)
		  nop)
      (lexical-context-pop-block ctxt))))

(defun compile4-return-from (expr ctxt)
  (assert (eq (car expr) 'return-from))
  (let ((block-name (cadr expr)))
    (let ((block-id (lexical-context-lookup-block ctxt block-name)))
      `(push ,block-id
	     ,@(compile4 `(progn ,@(cddr expr)) ctxt)
	     push 2
	     raise))))

(defun compile4-condition-case-handler (expr+tag mapcar-context)
  (let ((expr (car expr+tag))
	(tag (cddr expr+tag))
        (condition (cadr expr+tag))
	(ctxt (first mapcar-context))
	(jmp-target (second mapcar-context))
	(varname (third mapcar-context)))
    `((label ,tag)
      push ,condition
      swap
      push 2
      push cons
      call
      push 1
      ,@(compile4-lambda `(lambda (,varname) (progn ,@expr)) ctxt)
      call
      jmp (target ,jmp-target))))

(defun zip (list1 list2)
  (assert (= (length list1) (length list2)))
  (%zip list1 list2))

(defun %zip (list1 list2)
  (if (null list1)
      nil
      (cons (cons (car list1) (car list2)) (%zip (cdr list1) (cdr list2)))))

(defun compile4-condition-case (expr ctxt)
  (assert (eq (car expr) 'condition-case))
  (let ((e (cadr expr))
        (body1 `(funcall #'(lambda () ,(caddr expr))))
	(body (caddr expr))
	(handlers (cadr (cddr expr))))
    (let ((tag-alist (mapcar #'(lambda (handler)
				 (cons (car handler) (gensym)))
			     handlers)))
      (let ((set-tags (apply #'append
			     (mapcar #'(lambda (pair)
					 `(set-tag ,(car pair)
						   (target ,(cdr pair))))
				     tag-alist)))
	    (jmp-target (gensym))
	    (compiled-body (compile4 body1 ctxt)))
	(let ((compiled-handlers
	       (apply #'append
	       (mapcar-with-context #'compile4-condition-case-handler
				    (zip (mapcar #'cdr handlers) tag-alist)
				    (list ctxt jmp-target e)))))
	  `(,@set-tags
	    ,@compiled-body
	    jmp (target ,jmp-target)
	    ,@compiled-handlers
	    (label ,jmp-target)
;	    push nil
            ))))))

(defun compile4-function-call (expr ctxt)
  (assert (and (consp expr) (symbolp (car expr))))
  (let ((sym (car expr))
        (arg-count 0)
	(result nil))
    (dolist (arg (cdr expr))
      (setq result (append result (compile4 arg ctxt)))
      (setq arg-count (+ 1 arg-count)))
    (let ((pass-arg-count `(push ,arg-count))
	  (more-stuff `(push ,sym call)))
      (setq result (append (append result pass-arg-count) more-stuff))
      result)))

(defparameter *depth* 0)

(defparameter *trace-compile* nil)

(defun compile4 (expr ctxt)
  (when *trace-compile*
    (dotimes (i *depth*)
      (princ " "))
    (print expr)
    (incf *depth*))
  (let ((result (%compile4 expr ctxt)))
    (when *trace-compile*
      (dotimes (i *depth*)
        (princ " "))
      (print result)
      (incf *depth* -1))
    result))

(defun %compile4 (expr ctxt)
  (cond ((atom expr) 
	 (cond ((or (stringp expr) (integerp expr) (eq t expr) (eq nil expr))
		`(push ,expr))
	       ((symbolp expr)
		(let ((lookup-result (lexical-context-lookup ctxt expr)))
		  (if lookup-result
		      (progn
;;			(print (list expr lookup-result))
			`(get ,@lookup-result))
		      `(push ,expr push 1 push symbol-value call))))
	       (t `(push ,expr))))
	((symbolp (car expr))
	 (let ((sym (car expr)))
	   (cond ((eq sym 'function)
		  (let ((function (cadr expr)))
		    (if (symbolp function)
			`(push ,function)
			(compile4-lambda function ctxt))))
		 ((eq sym 'progn)
		  (compile4-progn expr ctxt))
		 ((eq sym 'quote)
		  `(push (literally ,(cadr expr))))
		 ((eq sym 'if)
		  (compile4-if expr ctxt))
		 ((eq sym 'let)
		  (compile4-let expr ctxt))
		 ((eq sym 'tagbody)
		  (compile4-tagbody expr ctxt))
		 ((eq sym 'go)
		  (compile4-go expr ctxt))
		 ((eq sym '%asm)
		  (compile4-asm expr ctxt))
		 ((eq sym 'set)
		  (compile4-set expr ctxt))
		 ((eq sym 'block)
		  (compile4-block expr ctxt))
		 ((eq sym 'return-from)
		  (compile4-return-from expr ctxt))
		 ((eq sym 'condition-case)
		  (compile4-condition-case expr ctxt))
		 (t (compile4-function-call expr ctxt)))))
	(t (raise 'bad-expression expr))))

;; This is showing up on the stack, not sure that is expected!
(defun macroexpand-1 (form)
  (let ((result (%macroexpand-1 form)))
    result))

(defun %macroexpand-1 (form)
  (when (not (consp form))
    (return-from %macroexpand-1 (cons form nil)))
  (if (and (symbolp (car form)) (get (car form) 'macro))
      (cons (apply (car form) (cdr form)) t)
      (cons form nil)))

(defun macroexpand (form)
  (let ((expanded t))
    (while expanded
      (let ((result (macroexpand-1 form)))
	(setq expanded (cdr result))
	(setq form (car result))))
    form))

(defun macroexpand-all-if (form)
  (assert (eq (car form) 'if))
  (let ((len (length form)))
    (if (= len 4)
	`(if ,(macroexpand-all (second form))
	     ,(macroexpand-all (third form))
	     ,(macroexpand-all (fourth form)))
	`(if ,(macroexpand-all (second form))
	     ,(macroexpand-all (third form))))))

(defun macroexpand-all-tagbody (form)
  (assert (eq (car form) 'tagbody))
  `(tagbody ,@(mapcar #'(lambda (e)
			  (if (symbolp e)
			      e
			      (macroexpand-all e)))
		      (cdr form))))

(defun macroexpand-all-list (list)
  (if (null list)
      nil
      (cons (macroexpand-all (car list)) (macroexpand-all-list (cdr list)))))

(defun macroexpand-all-let (clauses)
  (when (null clauses)
    (return-from macroexpand-all-let nil))
  (let ((clause (first clauses)))
    (if (consp clause)
	(let ((var (first clause))
	      (val (second clause)))
	  (cons `(,var ,(macroexpand-all val))
		(macroexpand-all-let (cdr clauses))))
	(cons clause (macroexpand-all-let (cdr clauses))))))

(defun macroexpand-all-condition-case (form)
  (assert (eq (first form) 'condition-case))
  (let ((exc (cadr form))
	(body (caddr form))
	(clauses (cdr (cddr form))))
    `(condition-case ,exc
		     ,(macroexpand-all body) ,(macroexpand-all-let clauses))))

(defun macroexpand-all-quasiquote (form depth)
  (assert (> depth 0))
  (when (or (symbolp form) (integerp form))
    (return-from macroexpand-all-quasiquote form))
  (cond ((consp form)
	 (if (symbolp (car form))
	     (let ((symbol (car form)))
	       (cond ((eq symbol _quasiquote)
		      `(,symbol (macroexpand_all_quasiquote form (+ depth 1))))
		     ((or (eq symbol _unquote)
			  (eq symbol _unquote-splice))
		      (if (= depth 1)
			  `(,symbol ,(macroexpand-all (cadr form)))
			  `(,symbol ,(macroexpand-all-quasiquote (cadr form)
								(- depth 1)))))
		     (t (cons (macroexpand-all-quasiquote (car form) depth)
			      (macroexpand-all-quasiquote (cdr form) depth)))))
	     (cons (macroexpand-all-quasiquote (car form) depth)
		   (macroexpand-all-quasiquote (cdr form) depth))))
	((vectorp form)
	 (raise 'not-implemented))
	(t form)))

(defun macroexpand-all-function (form)
  (assert (eq (car form) 'function))
  (when (symbolp (cadr form))
    (return-from macroexpand-all-function form))
  (if (and (consp (cadr form)) (eq (caadr form) 'lambda))
      (let ((lambda-expression (cadr form)))
	(let ((arglist (cadr lambda-expression))
	      (body (cddr lambda-expression)))
	  `(function (lambda ,arglist ,@(macroexpand-all-list body)))))
      (raise 'bad-function)))

(defun macroexpand-all (form)
  (setq form (macroexpand form))
  (if (not (consp form))
      (return-from macroexpand-all form))
  (if (not (symbolp (car form)))
      (return-from macroexpand-all (macroexpand-all-list form)))
  (let ((sym (car form)))
    (cond ((eq sym 'if)
	   (macroexpand-all-if form))
	  ((eq sym 'tagbody)
	   (macroexpand-all-tagbody form))
	  ((eq sym 'progn)
	   `(progn ,@(macroexpand-all-list (cdr form))))
	  ((eq sym 'condition-case)
	   (macroexpand-all-condition-case form))
	  ((eq sym 'let)
	   (let ((clauses (second form))
		 (body (cdr (cdr form))))
	     `(let ,(macroexpand-all-let clauses)
		,@(macroexpand-all-list body)))) ;not working?
	  ((eq sym 'quote) form)
	  ((eq sym 'quasiquote)
	   `(,sym ,@(macroexpand-all-quasiquote (cdr form) 1)))
	  ((eq sym 'function)
	   (macroexpand-all-function form))
	  (t `(,sym ,@(macroexpand-all-list (cdr form)))))))

;; Ultimately this should simply be called `compile`
(defun compile4-toplevel (expr)
  (let ((ctxt (make-lexical-context)))
    (let (foo)
      (condition-case e
	  (progn
	    (setq foo (macroexpand-all (convert-quasiquote expr 0)))
	    (assemble (compile4 (convert-quasiquote foo 0) ctxt)))
	(assertion-failed (print (list foo expr)))
	(bad-function (print (list foo expr)))))))
