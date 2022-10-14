(condition-case eof
    (prog (input)
     repl
     (princ "> ")
     (set 'input (read))
     (condition-case e
	 (print (eval input))
       (type-error (print e)))
     (go repl))
  (end-of-file
   (exit 0)))
