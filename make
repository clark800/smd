#!/bin/sh

"${CC-cc}" -std=c99 -Wpedantic -Wall -Wextra -Wshadow -Wcast-qual \
    -Wmissing-prototypes -Wstrict-prototypes -Wredundant-decls \
    -O2 $CPPFLAGS $CFLAGS $LDFLAGS -o smd *.c

if [ "$1" = "install" ]; then
    PREFIX="${PREFIX:-/usr/local}"
    mkdir -p "$PREFIX/bin"
    mv smd "$PREFIX/bin/"
fi
