(condition-case eof
    (let (input)
      (tagbody
	 (princ "Welcome to Graham's Lisp\n")
       repl
	 (princ "> ")
	 (setq input (read))
	 (condition-case e
             (progn
	       (setq * (eval input))
	       (print *))
	   (runtime-error (print e))
	   (unbound-variable (print e))
	   (type-error (print e)))
	 (setq + input)
	 (go repl)))
  (end-of-file
   (exit 0)))
