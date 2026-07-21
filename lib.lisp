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
    (return-from / first))
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
		  (when (not (equalp (svref a i) (svref b i)))
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
	  (when (= ,var ,max)
	    (go done))
	  ,@thing
	  (setq ,var (+ 1 ,var))
	  (go iterate)
	done))))

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

(defmacro while (condition &body body)
  (let ((iterate-tag (gensym))
	(out-tag (gensym)))
    `(tagbody
	,iterate-tag
	(if ,condition
	    (progn
	      ,@body
	      (go ,iterate-tag))
	    (go ,out-tag))
	,out-tag)))

(defmacro prog (varlist &body body)
  `(block nil
     (let ,varlist
       (tagbody ,@body))))

(defmacro prog1 (&body forms)
  (let ((first (car forms))
	(result (gensym)))
    `(progn
       (let ((,result ,first))
	 ,@(cdr forms)
	 ,result))))
    
(defmacro return (&optional value)
  `(return-from nil ,value))

(defmacro lambda (arglist &body body)
  `(function (lambda ,arglist ,@body)))

(defun null (x)
  (eq x nil))

(defun merge (list1 list2 comparator)
  (when (null list1)
    (return-from merge list2))
  (when (null list2)
    (return-from merge list1))
  (if (funcall comparator (car list1) (car list2))
      (cons (car list1) (merge (cdr list1) list2 comparator))
      (cons (car list2) (merge list1 (cdr list2) comparator))))

(defun copy-sublist (list n)
  (if (= n 0) nil
      (cons (car list) (copy-sublist (cdr list) (- n 1)))))

(defun nthcdr (n list)
  (if (= n 0) list
      (nthcdr (- n 1) (cdr list))))

(defun sort (list comparator)
  (let ((length (length list)))
    (when (eq length 1)
      (return-from sort list))
    (let ((list1 (copy-sublist list (/ length 2)))
	  (list2 (nthcdr (/ length 2) list)))
      (merge (sort list1 comparator) (sort list2 comparator) comparator))))


(defparameter * nil)
(defparameter ** nil)
(defparameter *** nil)
(defparameter + nil)
(defparameter ++ nil)
(defparameter +++ nil)

(defmacro assert (thing-that-should-be-true)
  `(when (not ,thing-that-should-be-true)
     (raise 'assertion-failed ',thing-that-should-be-true)))

(defun append (&rest lists)
  (let (result tail)
    (dolist (list lists)
      (dolist (item list)
	(let ((new-pair (cons item nil)))
	  (if result
	      (rplacd tail new-pair)
	      (setq result new-pair))
	  (setq tail new-pair))))
    result))

(defun caar (x)
  (car (car x)))

(defun cadr (x)
  (car (cdr x)))

(defun cdar (x)
  (cdr (car x)))

(defun cddr (x)
  (cdr (cdr x)))

(defun caddr (x)
  (car (cddr x)))

(defun caadr (x)
  (car (cadr x)))

(defun cdddr (x)
  (cdr (cddr x)))

(defun cadddr (x)
  (car (cdr (cddr x))))

(defun first (list)
  (car list))

(defun second (list)
  (cadr list))

(defun third (list)
  (caddr list))

(defun fourth (list)
  (cadddr list))

(defun assoc (item alist)
  (if (null alist)
      nil
      (if (eq item (caar alist))
	  (car alist)
	  (assoc item (cdr alist)))))

(defun find (item list)
  (if (null list)
      nil
      (if (equalp item (car list))
	  (car list)
	  (find item (cdr list)))))

(defun mapcar (function list)
  (if (null list)
      nil
      (cons (funcall function (car list)) (mapcar function (cdr list)))))

;; Because we don't have closures in the interpreted language
(defun mapcar-with-context (function list context)
  (if (null list)
      nil
      (cons (funcall function (car list) context)
	    (mapcar-with-context function (cdr list) context))))

(defun %reverse-aux (list acc)
  (if (null list)
      acc
      (%reverse-aux (cdr list) (cons (car list) acc))))

(defun reverse (list)
  (%reverse-aux list nil))

(defmacro incf (v &optional delta)
  (when (null delta)
    (setq delta 1))
  `(setq ,v (+ ,v ,delta)))

(defun remove-if-not (fn list)
  (if (null list)
      nil
      (if (funcall fn (car list))
	  (cons (car list) (remove-if-not fn (cdr list)))
	  (remove-if-not fn (cdr list)))))

(defun adjoin (item list &optional test)
  (when (null test)
    (setq test #'eq))
  (let (foundp)
    (dolist (obj list)
      (when (funcall test obj)
	(setq foundp t)))
    (if foundp
	list
	(cons item list))))

(defmacro push (obj place)
  `(setq ,place (cons ,obj ,place)))

(defun vector (&rest objects)
  (let ((len (length objects)))
    (let ((result (make-vector len)))
      (let ((i 0))
	(dolist (obj objects)
	  (set-svref result i obj)
	  (incf i)))
      result)))

(defun list-to-vector (list)
  (let ((length (length list)))
    (let ((vector (make-vector length))
	  (i 0))
      (dolist (obj list)
	(set-svref vector i obj)
	(setq i (+ 1 i)))
      vector)))

(defun %defstruct-make-accessors (struct-name slot-name index)
  `(progn
     (defun ,(make-symbol (%strconcat (%strconcat struct-name "-")
                                      slot-name))
         (obj)
       (svref obj ,index))
     (defun ,(make-symbol (%strconcat (%strconcat struct-name "-set-")
                                      slot-name))
         (obj new-value)
       (set-svref obj ,index new-value))))

(defun %defstruct-make-initializer (struct-name slots)
  (let ((index 2))
    (let ((cond-clauses
           (mapcar #'(lambda (slot)
                       (let ((keyword
                              (make-symbol
                               (%strconcat ":" (symbol-name slot)))))
                         (prog1
                             `((eq (car args) ,keyword)
                               (progn
                                 (push ',slot provided-args)
                                 (set-svref result ,index (cadr args))
                                 (setq args (cddr args))))
                           (incf index))))
                   slots)))
      `(let (provided-args)
         (tagbody
          :next-arg
            (cond ,@cond-clauses)
            (when (not (null args))
              (go :next-arg)))
         provided-args))))

(defmacro defstruct (struct-name slots)
  (let ((name (symbol-name struct-name))
        (slot-names (mapcar #'(lambda (slot)
                                (if (consp slot)
                                    (car slot)
                                    slot)) 
                            slots))
        (n (length slots)))
    (let (default-initializer-map
          (i 2))
      (dolist (slot slots)
        (when (consp slot)
          (let ((slot-name (car slot))
                (slot-initializer (cadr slot)))
            (push `(cons ',slot-name
                         (cons ,i
                               ,(if (or (eq slot-initializer t)
                                        (eq slot-initializer nil)
                                        (integerp slot-initializer))
                                    slot-initializer
                                    `#'(lambda () ,slot-initializer))))
                  default-initializer-map)))
        (incf i))
      `(progn
         (defparameter ,struct-name (list ,@default-initializer-map))
         (defun ,(make-symbol (%strconcat "make-" name)) (&rest args)
           (let ((result (make-vector ,(+ 2 n))))
             (set-svref result 0 :struct)
             (set-svref result 1 ',struct-name)
             (let ((provided-args
                    ,(%defstruct-make-initializer struct-name slot-names)))
               (dolist (thing ,struct-name)
                 (let ((thing-slot (car thing))
                       (thing-idx (cadr thing))
                       (thing-initializer (cddr thing)))
                   (when (not (find thing-slot provided-args))
                     (set-svref result thing-idx
                                (if (functionp thing-initializer)
                                    (funcall thing-initializer)
                                    thing-initializer)))))
               result)))
         ,@(let ((context (cons 2 name)))
             (mapcar-with-context
              #'(lambda (slot-name context)
                  (prog1
                      (%defstruct-make-accessors (cdr context)
                                                 (symbol-name slot-name)
                                                 (car context))
                    (rplaca context (+ 1 (car context)))))
              slot-names context))
         ',struct-name))))
