;; Implementation of eval for the stack machine
(defun eval (expr)
  (let ((compiled-expr (compile-toplevel expr)))
    (let ((len (length compiled-expr)))
      (let ((new-code (make-vector (two-arg-plus len 1)))
            (j 0))
        (tagbody
	 loop
	   (set-svref new-code j (svref compiled-expr j))
	   (setq j (two-arg-plus j 1))
	   (if (eq j len)
	       (go last-bit))
	   (go loop)
	 last-bit
           ;; Append ret instruction
	   (set-svref new-code j 8))
        (funcall (%vm-make-simple-function #(nil 0) new-code))))))
