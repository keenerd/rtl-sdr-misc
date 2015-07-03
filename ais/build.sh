#!/bin/sh
# todo, a real makefile

#point this to the correct path
RTLSDR_PATH="/tmp/rtl-sdr-exp/src"

files="rtl_ais.c convenience.c \
		./aisdecoder/aisdecoder.c ./aisdecoder/sounddecoder.c \
		./aisdecoder/lib/receiver.c 
		./aisdecoder/lib/protodec.c
		./aisdecoder/lib/hmalloc.c
		./aisdecoder/lib/filter.c "
		
flags="-Wall -O2 "
includes="-I/usr/include/libusb-1.0 -I./aisdecoder -I ./aisdecoder/lib"
libs="-L/usr/lib -L. -lusb-1.0 -lrtlsdr -lpthread -lm "

UNAME=$(uname)
if [ "$UNAME" != "Linux" ]
then
# Conditional section for Windows
	libs="$libs -lWs2_32"
fi

rm -f rtl_ais
echo gcc -o rtl_ais $files $flags $includes $libs 
gcc -o rtl_ais $files $flags $includes $libs 

