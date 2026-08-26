#!/usr/bin/env bash

./main --heap-size=128m --use-vm --vmboot=vmboot.compiled --eval='(save-image "image.image")' lib.compiled compiler.compiled apply.compiled eval.compiled load.compiled raise.compiled repl.compiled
