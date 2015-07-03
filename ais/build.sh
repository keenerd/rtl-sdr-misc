#!/bin/sh
# todo, a real makefile

#point this to the correct path
RTLSDR_PATH="../../src"

files="$RTLSDR_PATH/rtl_ais.c $RTLSDR_PATH/convenience/convenience.c \
		$RTLSDR_PATH/aisdecoder/aisdecoder.c $RTLSDR_PATH/aisdecoder/sounddecoder.c \
		$RTLSDR_PATH/aisdecoder/lib/receiver.c 
		$RTLSDR_PATH/aisdecoder/lib/protodec.c
		$RTLSDR_PATH/aisdecoder/lib/hmalloc.c
		$RTLSDR_PATH/aisdecoder/lib/filter.c "
		
flags="-Wall -O2 "
includes="-I/usr/include/libusb-1.0 -I../../include -I ../../src/convenience -I ../../src/aisdecoder -I ../../src/aisdecoder/lib"
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

