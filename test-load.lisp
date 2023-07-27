;; Define a function
(set-symbol-function 'test1 #'(lambda (x) (cons (quote hello) x)))

;; Call it
(test1 (quote world))

;; Define another function
(set-symbol-function 'test2 #'(lambda (x)
  (cond ((eq x (quote bof)) (quote boo))
	(t (quote ohno)))))

;; Test the function
(test2 (quote bof))

(test2 (quote foo))
