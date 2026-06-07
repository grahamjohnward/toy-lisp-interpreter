#!/usr/bin/bash

./main --heap-size=64m lib.lisp compiler.lisp bootstrap.lisp &&
time ./main --heap-size=128m --use-vm --vmboot=vmboot.compiled lib.compiled compiler.compiled apply.compiled bootstrap.compiled
