#!/usr/bin/env bash

D=$(dirname $0)

exec rlwrap $D/main --heap-size=128m -use-vm --image=image --eval='(repl)'
