#!/bin/sh

BIN="smd"

build() {
    "${CC-cc}" -std=c99 -Wpedantic -Wall -Wextra -Wshadow -Wcast-qual \
        -Wmissing-prototypes -Wstrict-prototypes -Wredundant-decls \
        -O2 $CPPFLAGS $CFLAGS $LDFLAGS -o "$BIN" *.c
}

install() {
    PREFIX="${PREFIX:-/usr/local}"
    mkdir -p "$PREFIX/bin"
    cp "$BIN" "$PREFIX/bin/"
}

case "$1" in
    "") build;;
    install) install;;
    *) exit 1;;
esac
