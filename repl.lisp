(condition-case eof
    (prog (input)
     repl
     (princ "> ")
     (set 'input (read))
     (condition-case e
	 (print (eval input))
       (unbound-variable (print e))
       (type-error (print e)))
     (go repl))
  (end-of-file
   (exit 0)))
