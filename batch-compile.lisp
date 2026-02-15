(progn
  (let ((source-file (svref *argv* 0))
        (compiled-file (svref *argv* 1)))
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
