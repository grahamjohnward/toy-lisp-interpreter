(condition-case eof
    (let (input)
      (tagbody
	 (princ "Welcome to Graham's Lisp (basic REPL)\n")
       repl
	 (princ "> ")
	 (set 'input (read))
	 (condition-case e
	     (print (eval input))
	   (runtime-error (print e))
	   (unbound-variable (print e))
	   (type-error (print e)))
	 (go repl)))
  (end-of-file
   (exit 0)))
