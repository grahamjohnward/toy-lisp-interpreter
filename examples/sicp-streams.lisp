(let ((unevaluated 'unevaluated))
  (defun memoize (fn)
    (let ((value unevaluated))
      #'(lambda ()
          (when (eq unevaluated value)
            (setq value (funcall fn)))
          value))))

(defmacro delay (x)
  `(memoize #'(lambda () ,x)))

(defun force (thunk)
  (funcall thunk))

(defun test-force-delay ()
  (let ((thunk (delay 13)))
    (assert (= 13 (force thunk)))
    (assert (= 13 (force thunk))))
  (let ((thunk (delay (+ 2 2))))
    (assert (= 4 (force thunk)))
    (assert (= 4 (force thunk))))
  (let ((x 9))
    (let ((thunk (delay (* x x))))
      (assert (= 81 (force thunk)))
      (assert (= 81 (force thunk)))))
  (let ((x 0))
    (let ((thunk (delay (incf x))))
      (force thunk)
      (assert (= x 1))
      (force thunk)
      (assert (= x 1))))
  :ok)

(defmacro cons-stream (x y)
  `(cons ,x (delay ,y)))

(defun stream-car (x)
  (car x))

(defun stream-cdr (x)
  (force (cdr x)))

(defparameter the-empty-stream 'the-empty-stream)

(defun stream-null-p (s)
  (eq s the-empty-stream))

(defun stream-ref (s n)
  (if (stream-null-p s)
      (values nil nil)
      (if (= 0 n)
          (stream-car s)
          (stream-ref (stream-cdr s) (1- n)))))

(defun stream-enumerate-interval (low high)
  (if (> low high)
      the-empty-stream
      (cons-stream low (stream-enumerate-interval (+ low 1) high))))

(defun stream-map (fn s)
  (if (stream-null-p s)
      the-empty-stream
      (cons-stream (funcall fn (stream-car s)) (stream-map fn (stream-cdr s)))))

(defun stream-map2 (fn &rest streams)
  (if (stream-null-p (car streams))
      the-empty-stream
      (cons-stream
       (apply fn (mapcar #'stream-car  streams))
       (apply #'stream-map2
              (cons fn (mapcar #'stream-cdr streams))))))

(defun stream-filter (pred s)
  (if (stream-null-p s)
      the-empty-stream
      (if (funcall pred (stream-car s))
          (cons-stream (stream-car s) (stream-filter pred (stream-cdr s)))
          (stream-filter pred (stream-cdr s)))))

(defun stream-for-each (fn s)
  (when (not (stream-null-p s))
    (funcall fn (stream-car s))
    (stream-for-each fn (stream-cdr s))))

;; Returns a list, not a stream
(defun take (n stream)
  (if (= n 0) nil
      (cons (stream-car stream) (take (1- n) (stream-cdr stream)))))


(defun integers-starting-from (n)
  (cons-stream n (integers-starting-from (+ n 1))))

(defun integers () (integers-starting-from 1))

(defun divisible-p (x y) (= (mod x y) 0))

(defun sieve (stream)
  (cons-stream
   (stream-car stream)
   (sieve (stream-filter #'(lambda (x)
                             (not (divisible-p x (stream-car stream))))
                         (stream-cdr stream)))))
