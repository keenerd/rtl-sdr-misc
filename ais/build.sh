#!/bin/sh

# todo, a real makefile

files="rtl_ais.c convenience.c"
flags="-Wall -O2"
includes="-I/usr/include/libusb-1.0"
libs="-lusb-1.0 -lrtlsdr -lpthread -lm"

rm -f rtl_ais
gcc -o rtl_ais $files $flags $includes $libs

