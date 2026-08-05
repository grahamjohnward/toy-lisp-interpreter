(defun load (source-file)
  (let ((in-stream (open source-file :read)))
    (condition-case eof
	(let (input)
	  (tagbody
	   loop
	     (setq input (read1 in-stream))
	     (let ((code (compile4-toplevel input)))
               (let ((len (length code)))
                 (let ((new-code (make-vector (two-arg-plus len 1)))
		       (j 0))
		   (tagbody
		    loop2
		      (set-svref new-code j (svref code j))
		      (setq j (two-arg-plus j 1))
		      (if (eq j len)
			  (go last-bit))
		      (go loop2)
		    last-bit
		      (set-svref new-code j 8))
		   (print (funcall (%vm-make-simple-function #(nil 0) new-code)))))
	       (go loop))))
      (end-of-file t))))
