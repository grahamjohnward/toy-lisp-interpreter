(progn
  (defun %bog (list)
    (let ((l (length list)))
      (let ((i (two-arg-minus l 1))
            (result (make-vector l)))
        (tagbody
         next
           (if (eq list nil)
               (go done))
           (set-svref result i (car list))
           (setq list (cdr list))
           (setq i (two-arg-minus i 1))
           (go next)
         done)
        result)))

  (defun load-compiled-file (s)
    (let ((stream (open s :read)))
      (condition-case e
	  (tagbody
	   next1
	     (let ((code (read1 stream)))
	       (let ((len (length code)))
	         ;; Append ret instruction
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
                   ;; XXX why is this using funcall??
		   (funcall (%vm-make-simple-function #(nil 0) new-code)))))
	     (go next1))
        (end-of-file
         't))))

  (defun %load-compiled-files (x)
    (putprop '*argv* 'param t)
    (let ((l (length x))
          (parsing-arguments nil)
          (arguments nil)
          (files-to-load nil)
          (argcount 0)
	  (i 0))
      (tagbody
       next
         (if (= l i)
	     (go done))
         (if (string-equal-p (svref x i) "--")
             (setq parsing-arguments t)
             (if (eq parsing-arguments nil)
                 (setq files-to-load (cons (svref x i) files-to-load))
                 (progn
                   (setq arguments (cons (svref x i) arguments))
                   (setq argcount (two-arg-plus argcount 1)))))
         (setq i (two-arg-plus i 1))
         (go next)
       done)
      (setq *argv* (%bog arguments))
      (setq files-to-load (%bog files-to-load))
      (let ((nfiles (length files-to-load))
            (i 0))
        (tagbody
         next
           (when (= i nfiles)
             (go done))
           (load-compiled-file (svref files-to-load i))
           (setq i (two-arg-plus i 1))
           (go next)
         done))))

  (%asm
   ;; Vector of command-line arguments is already on the stack
   push 1
   push %load-compiled-files
   call))
