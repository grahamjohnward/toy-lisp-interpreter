(progn
  (set-symbol-function 'defmacro
		       #'(lambda (name arglist &body body)
			   `(progn
			      (let ((result (set-symbol-function ',name #'(lambda ,arglist (block ,name ,@body)))))
				(putprop ',name 'macro 't)
				result))))
  (putprop 'defmacro 'macro 't))

(defmacro defun (fname arglist &body body)
  `(set-symbol-function ',fname #'(lambda ,arglist (block ,fname ,@body))))

(defmacro defparameter (name initial-value)
  `(progn
     (putprop ',name 'param t)
     (set-symbol-value ',name ,initial-value)
     ',name))

(defun list (&rest args)
  args)

(defmacro cond (&rest clauses)
  (let ((first-clause (car clauses)))
    (if (eq first-clause nil)
	nil
	`(if ,(car first-clause) ,(car (cdr first-clause))
	     ,(apply #'cond (cdr clauses))))))

(defmacro when (p &body a)
  `(if ,p
       (progn ,@a)))

(defmacro setq (var value)
  `(set ',var ,value))

(defun + (&rest args)
  (let (x total)
    (tagbody
       (setq total 0)
     iterate
       (when (eq nil args)
	 (return-from + total))
       (setq x (car args))
       (setq args (cdr args))
       (setq total (two-arg-plus total x))
       (go iterate))))

(defun - (&rest args)
  (when (eq nil (cdr args))
    (return-from - (two-arg-minus 0 (car args))))
  (let (x result)
    (tagbody
       (setq result (car args))
     iterate
       (setq args (cdr args))
       (when (eq nil args)
	 (return-from - result))
       (setq x (car args))
       (setq result (two-arg-minus result x))
       (go iterate))))

(defun * (&rest args)
  (let (x result)
    (tagbody
       (setq result 1)
     iterate
       (when (eq nil args)
	 (return-from * result))
       (setq x (car args))
       (setq args (cdr args))
       (setq result (two-arg-times result x))
       (go iterate))))

(defun / (first &rest args)
  (when (eq args nil)
    (return first))
  (two-arg-divide first (apply #'* args)))

(defun not (x)
  (if x nil t))

(defun %and (things)
  (if (eq (cdr things) nil)
      `,(car things)
      `(if ,(car things) ,(%and (cdr things)))))
  
(defmacro and (&rest things)
  (%and things))

(defun %or (things)
  (if (eq (cdr things) nil)
      `,(car things)
      (let ((x (car things)))
	`(if ,x ,x ,(%or (cdr things))))))

(defmacro or (&rest things)
  (%or things))

 (defun equalp (a b)
   (when (not (eq (type-of a) (type-of b)))
     (return-from equalp nil))
   (cond ((eq (type-of a) 'string)
	  (string-equal-p a b))
	 ((eq (type-of a) 'vector)
	  (progn
	    (when (not (eq (length a) (length b)))
	      (return-from equalp nil))
	    (return-from equalp
	      (let (i)
		(tagbody
		   (setq i 0)
		 iterate
		   (when (eq i (length a))
		     (return-from equalp t))
		   (when (not (eq (svref a i) (svref b i)))
		     (return-from equalp nil))
		   (setq i (+ i 1))
		   (go iterate))))))
	 ((and (eq (type-of a) 'cons) (eq (type-of b) 'cons))
	  (if (equalp (car a) (car b))
	      (return-from equalp (equalp (cdr a) (cdr b)))
	      (return-from equalp nil)))
	 (t (eq a b))))

(defun > (first &rest rest)
  (if (eq rest nil)
      (return-from > (eq (type-of first) 'integer))
      (if (two-arg-greater-than first (car rest))
	  (apply '> rest))))

(defun < (first &rest rest)
  (if (eq rest nil)
      (return-from < (eq (type-of first) 'integer))
      (if (two-arg-less-than first (car rest))
	  (apply '< rest))))

(defmacro dotimes (var-and-max &body thing)
  (let ((var (car var-and-max))
	  (max (car (cdr var-and-max))))
      `(let ((,var nil))
	 (tagbody
	    (setq ,var 0)
	  iterate
	    ,@thing
	    (setq ,var (+ 1 ,var))
	    (when (< ,var ,max)
	      (go iterate))))))

(defmacro dolist (args &body body)
  (let ((var (car args))
	(list (car (cdr args)))
	(list-var (gensym)))
    `(let ((,var (car ,list))
	   (,list-var ,list))
       (tagbody
	iterate
	  (if (not (eq ,list-var nil))
	      (progn
		(setq ,var (car ,list-var))
		,@body
		(setq ,list-var (cdr ,list-var))
		(go iterate)))))))


(defmacro prog (varlist &body body)
  `(block nil
     (let ,varlist
       (tagbody ,@body))))

(defmacro return (&optional value)
  `(return-from nil ,value))

(defmacro lambda (arglist &body body)
  `(function (lambda ,arglist ,@body)))
