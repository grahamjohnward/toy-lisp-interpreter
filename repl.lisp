(condition-case eof
    (let (input)
      (tagbody
	 (print "Welcome to Graham's Lisp")
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
