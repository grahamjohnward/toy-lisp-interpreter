(progn
  (set-symbol-function
   'load-compiled-file
   #'(lambda (s)
       (let ((stream (open s :read)))
	 (condition-case e
	     (tagbody
	      next1
		(let ((code (read1 stream)))
		  (let ((len (length code)))
		    ;; Append ret instruction
		    (let ((new-code (make-vector (two-arg-plus len 1)))
			  (j 0))
		      (tagbody
		       loop
			 (set-svref new-code j (svref code j))
			 (setq j (two-arg-plus j 1))
			 (if (eq j len)
			     (go last-bit))
			 (go loop)
		       last-bit
			 (set-svref new-code j 'ret))
		      ;; We don't want this to make a closure:
		      (funcall (%vm-make-simple-function #(nil 0) new-code)))))
		(go next1))
	   (end-of-file
	    t)))))
  (set-symbol-function
   '%load-compiled-files
   #'(lambda (x)
       (let ((l (length x))
	     (i 0))
	 (tagbody
	  next
	    (if (= l i)
		(go done))
	    (load-compiled-file (svref x i))
	    (setq i (two-arg-plus i 1))
	    (go next)
	  done))))
  (%asm
   ;; Vector of command-line arguments is already on the stack
   push 1
   push %load-compiled-files
   call))
