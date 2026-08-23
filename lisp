#!/usr/bin/env bash

D=$(dirname $0)


HEAP_SIZE=${HEAP_SIZE:-128m}

exec rlwrap $D/main --heap-size=$HEAP_SIZE -use-vm --image=image --eval='(repl)'
