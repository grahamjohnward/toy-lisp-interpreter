(prog (input)
 repl
 (princ "> ")
 (set 'input (read))
 (print (eval input))
 (go repl))
