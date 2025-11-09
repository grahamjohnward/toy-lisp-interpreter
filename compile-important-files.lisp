(let ((important-files '(("lib.lisp" . "lib.compiled")
			 ("compiler.lisp" . "compiler.compiled")
;			 ("apply.lisp" . "apply.compiled")
			 ("test-script.lisp" . "test-script.compiled")
;			 ("vm-test-apply.lisp" . "vm-test-apply.compiled")
			 ("vmboot.lisp" . "vmboot.compiled"))))
  (tagbody
   next-file
     (if (eq important-files nil)
	 (go done))
     (let ((file (car important-files)))
       (print file)
       (let ((source-file (car file))
	     (compiled-file (cdr file)))
	 (let ((in-stream (open source-file :read))
	       (out-stream (open compiled-file :write)))
	   (condition-case eof
	       (let (input)
		 (tagbody
		  loop
		    (setq input (read1 in-stream))
		    (write (compile4-toplevel input) out-stream)
		    (go loop)))
	     (end-of-file nil)))))
     (setq important-files (cdr important-files))
     (go next-file)
   done))
