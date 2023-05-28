(defun list (&rest args)
  args)

(defmacro if (p a &optional b)
  (cond (b `(cond (,p ,a) (t ,b)))
	(t `(cond (,p ,a) (t nil)))))

(defmacro when (p &body a)
  `(if ,p
       (progn ,@a)))

(defun + (&rest args)
  (prog (x total)
     (set 'total 0)
   iterate
     (when (eq nil args)
       (return total))
     (set 'x (car args))
     (set 'args (cdr args))
     (set 'total (two-arg-plus total x))
     (go iterate)))

(defun - (&rest args)
  (progn
    (when (eq nil (cdr args))
      (return (two-arg-minus 0 (car args))))
    (prog (x result)
       (set 'result (car args))
       iterate
       (set 'args (cdr args))
       (when (eq nil args)
	 (return result))
       (set 'x (car args))
       (set 'result (two-arg-minus result x))
       (go iterate))))

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
   (progn
     (when (not (eq (type-of a) (type-of b)))
       (return nil))
     (cond ((eq (type-of a) 'string)
	    (string-equal-p a b))
	   ((eq (type-of a) 'vector)
	    (progn
	      (when (not (eq (length a) (length b)))
		(return nil))
	      (return
		(prog (i)
		   (set 'i 0)
		   iterate
		   (when (eq i (length a))
		     (return t))
		   (when (not (eq (svref a i) (svref b i)))
		     (return nil))
		   (set 'i (+ i 1))
		   (go iterate)))))
	   (t (eq a b)))))

(defun > (first &rest rest)
  (progn
    (if (eq rest nil)
	(return (eq (type-of first) 'integer))
	(if (two-arg-greater-than first (car rest))
	    (apply '> rest)))))

(defun < (first &rest rest)
  (progn
    (if (eq rest nil)
	(return (eq (type-of first) 'integer))
	(if (two-arg-less-than first (car rest))
	    (apply '< rest)))))

(defmacro dotimes (var-and-max &body thing)
  (let ((var (car var-and-max))
	  (max (car (cdr var-and-max))))
      `(let ((,var nil))
	 (tagbody
	    (set ',var 0)
	  iterate
	    ,@thing
	    (set ',var (+ 1 ,var))
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
		(set ',var (car ,list-var))
		,@body
		(set ',list-var (cdr ,list-var))
		(go iterate)))))))
