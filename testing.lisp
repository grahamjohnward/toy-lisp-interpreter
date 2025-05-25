(defmacro do-test (expression expected-value)
  `(progn
     (setq test-count (+ 1 test-count))
     (let ((v ,expression))
     (if (equalp v ,expected-value)
; 	 (print "ok")
	 (cons 'a 'b)
	 (progn
	   (setq fail-count (+ 1 fail-count))
	   (princ "Test failed\n")
	   (print ',expression)
	   (princ "Expected:\n")
	   (print ,expected-value)
	   (princ "Got:\n")
	   (print v))))))

(defmacro do-tests (&body tests)
  `(let ((test-count 0)
	 (fail-count 0))
     (progn
       (progn
	 ,@tests)
       (if (= fail-count 0)
	   (princ "All tests successful\n")
	   (progn
	     (princ fail-count)
	     (princ "/")
	     (princ test-count)
	     (princ " test(s) failed\n")))
       (exit fail-count))))

