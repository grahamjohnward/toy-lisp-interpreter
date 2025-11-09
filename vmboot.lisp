;; This needs to get the filenames to load from an argument passed
;; from main.c

;; The idea is:
;;  1.  Batch-compile this file
;;  2.  Use this as a VM-boot script
;;  3.  For now, we can hardcode the list of files to load (?)
;; This version with the hardcoded filenames works
(funcall
 #'(lambda (x)
     (let ((l (length x))
	   (i 0))
       (tagbody
	next
	  (if (= l i)
	      (go done))
	  (let ((s (svref x i)))
	    (let ((stream (open s :read)))
	      (condition-case e
		  (tagbody
		   next1
		     (let ((code (read1 stream)))
		       (let ((len (length code)))
			 (let ((new-code (make-vector (two-arg-plus len 1)))
			       (j 0))
			   (tagbody
			    loop
			      (set-svref new-code j (svref code j))
			      (setq j (two-arg-plus j 1))
			      (if (eq j len)
				  (go last-bit))
			      (go loop)
			    last-bit
			      (set-svref new-code j 'ret))
			   ;; We don't want this to make a closure:
			   (funcall (%vm-make-simple-function #(nil 0) new-code)))))
		     (go next1))
		(end-of-file
		 (print (cons 'done s))))))
	  (setq i (two-arg-plus i 1))
	  (go next)
	done)))
 #(;;"vm-test-apply.compiled"
   "lib.compiled"
   "compiler.compiled"
   "test-script.compiled"
   ))
