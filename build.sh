#!/bin/bash

: ${CC=gcc}
: ${BIN=image.so}
CFLAGS="$CFLAGS -fPIC -Ilib/lite-xl/resources/include -Ilib/stb"
LDFLAGS="$LDFLAGS -lm"

[[ "$@" == "clean" ]] && rm -f *.so && exit 0

$CC $CFLAGS image.c $@ -shared -o $BIN $LDFLAGS
