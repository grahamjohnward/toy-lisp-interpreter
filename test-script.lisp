;(dotimes (i 10)
;  (print (list :bogboo i)))

;(print (sort (list 2 43 6 34 3 6 6 3 7 2) #'<))

					;(/ 4 2)

;(print (/ 4 2))

;(print (funcall #'funcall #'cons :it :worked))

;(print (funcall #'apply #'cons `(:it :worked2)))

;(defun wow (a b)
;  (cons :foobar (+ a b)))

;(print (wow 12 14))


;; Need to start breaking down compile4-toplevel


;;(print (compile4-toplevel '(let ((a 14) (b 12)) (+ a b))))

;; This is wrong

;(print `(bof ,@(cons :a nil)))
;;(print (cons :boobah (cons (macroexpand-all '(let ((a 14) (b 12)) (+ a b))) nil)))


;; This does not crash but is wrong:
;(print (cons :boggo (cons (convert-quasiquote (macroexpand-all '(let ((a 14) (b 12)) (+ a b)))) nil)))



(defun y (f)
  (funcall #'(lambda (g) (funcall g g))
	   #'(lambda (i)
	       (funcall f #'(lambda (x)
			      (apply (funcall i i) (list x)))))))

(defun fac (n)
  (funcall
   (y #'(lambda (f)
	  #'(lambda (n)
              (if (= 0 n)
		  1
		  (* n (funcall f (- n 1)))))))
   n))

(dotimes (i 10)
  (print (fac i)))
