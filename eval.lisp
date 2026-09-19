;; Implementation of eval for the stack machine
(defun %eval-make-function (bytecode)
  (let* ((len (length bytecode))
         (new-code (make-vector (two-arg-plus len 1)))
         (j 0))
    (tagbody
     loop
       (set-svref new-code j (svref bytecode j))
       (setq j (two-arg-plus j 1))
       (if (eq j len)
	   (go last-bit))
       (go loop)
     last-bit
       ;; Append ret instruction
       (set-svref new-code j 8))
    (%vm-make-function #(nil 0) new-code)))

(defun eval (expr)
  (funcall (%eval-make-function (compile-toplevel expr))))
