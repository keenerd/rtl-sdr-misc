#!/bin/sh

# todo, a real makefile

files="waterfall.c"
binary="waterfall"
flags="-Wall -O2"
includes="-I/usr/include/libusb-1.0"
libs="-lSDL -lSDL_image -lSDL_ttf -lusb-1.0 -lrtlsdr -lpthread -lm"

rm -f $binary
gcc -o $binary $files $flags $includes $libs

