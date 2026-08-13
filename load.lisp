(defun load (source-file)
  (let ((in-stream (open source-file :read)))
    (condition-case eof
	(let (input)
	  (tagbody
	   loop
	     (setq input (read1 in-stream))
             (eval input)
             (go loop)))
      (end-of-file t))))
