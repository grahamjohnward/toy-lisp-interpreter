;; Implementation of eval for the stack machine
(defun %eval-make-function (bytecode)
  (let ((len (length bytecode)))
    (let ((new-code (make-vector (two-arg-plus len 3)))
          (j 0))
      ;; Prepend make-env 0
      (set-svref new-code 0 14)
      (set-svref new-code 1 0)
      (tagbody
       loop
	 (set-svref new-code (+ j 2) (svref bytecode j))
	 (setq j (two-arg-plus j 1))
	 (if (eq j len)
	     (go last-bit))
	 (go loop)
       last-bit
         ;; Append ret instruction
	 (set-svref new-code (+ j 2) 8))
      (%vm-make-function #(nil 0) new-code))))

(defun eval (expr)
  (funcall (%eval-make-function (compile-toplevel expr))))
