#!/bin/bash

: ${CC=gcc}
: ${BIN=image.so}

CFLAGS="$CFLAGS -fPIC"
LDFLAGS="$LDFLAGS -lm"

[[ "$@" == "clean" ]] && rm -f $BIN && exit 0

[[ "$@" != *"-g"* ]] && CFLAGS="$CFLAGS -O2" && LDFLAGS="$LDFLAGS -s"

$CC $CFLAGS image.c $@ -shared -o $BIN $LDFLAGS
