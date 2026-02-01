(defun raise (tag value)
  (%asm
   get 0 1
   get 0 2
   push 2
   raise))
