;; Define a function
(DEFUN TEST1 (X) (CONS (QUOTE HELLO) X))

;; Call it
(TEST1 (QUOTE WORLD))

;; Define another function
(DEFUN TEST2 (X) 
  (COND ((EQ X (QUOTE BOF)) (QUOTE BOO))
	(T (QUOTE OHNO))))

;; Test the function
(TEST2 (QUOTE BOF))

(TEST2 (QUOTE FOO))
