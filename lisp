#!/usr/bin/env bash

D=$(dirname $0)

exec rlwrap $D/main --heap-size=128m -use-vm --vmboot=vmboot.compiled --eval='(repl)' lib.compiled compiler.compiled apply.compiled eval.compiled load.compiled raise.compiled repl.compiled
