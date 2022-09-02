;; Define a function
(defun test1 (x) (cons (quote hello) x))

;; Call it
(test1 (quote world))

;; Define another function
(defun test2 (x) 
  (cond ((eq x (quote bof)) (quote boo))
	(t (quote ohno))))

;; Test the function
(test2 (quote bof))

(test2 (quote foo))
