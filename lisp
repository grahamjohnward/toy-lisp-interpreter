#!/usr/bin/env bash

D=$(dirname $0)

exec rlwrap $D/main --image=image.image --eval='(repl)'
