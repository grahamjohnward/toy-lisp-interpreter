(let ((important-files '(("lib.lisp" . "lib.compiled")
                         ("batch-compile.lisp" . "batch-compile.compiled")
                         ("repl.lisp" . "repl.compiled")
			 ("compiler.lisp" . "compiler.compiled")
			 ("vmboot.lisp" . "vmboot.compiled")
                         ("eval.lisp" . "eval.compiled")
			 ("compile-important-files.lisp" . "compile-important-files.compiled")
			 )))
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
