(defun repl ()
  (condition-case eof
      (let (input result)
        (tagbody
	   (princ "Welcome to Graham's Lisp\n")
         repl
	   (princ "> ")
	   (setq input (read))
	   (condition-case e
               (progn
	         (setq *** **)
	         (setq ** *)
	         (setq * result)
	         (setq result (eval input))
	         (print result))
	     (runtime-error (print e))
	     (unbound-variable (print e))
             (bad-function (print e))
             (bad-arity (print e))
	     (type-error
              (progn
                (princ "Type error: ")
                (print e))))
	   (setq +++ ++)
	   (setq ++ +)
	   (setq + input)
	   (go repl)))
    (end-of-file
     (exit 0))))
