(let ((expr '(let ((x 10))
	      (tagbody
	       iterate
		 (if (= x 0)
		     (print "BOOM")
		     (progn
		       (print x)
		       (setq x (two-arg-minus x 1))
		       (go iterate))))
	      x)))
  (let ((compiled (compile4-toplevel expr)))
    (let ((result (vm-eval compiled)))
      (print (list :compiled compiled))
      (print (list :result result))
      :done)))
