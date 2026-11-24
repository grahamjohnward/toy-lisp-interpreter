#!/usr/bin/env bash

./main --heap-size=128m --vmboot=vmboot.compiled --eval='(save-image "image.image")' lib.compiled compiler.compiled apply.compiled eval.compiled load.compiled raise.compiled repl.compiled build.compiled
