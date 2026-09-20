#!/usr/bin/env bash

D=$(dirname $0)

exec rlwrap $D/main --use-vm --image=image.image --eval='(repl)'
