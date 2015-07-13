CFLAGS?=-O2 -g -Wall -W 
CFLAGS+= -I./aisdecoder -I ./aisdecoder/lib
LDFLAGS+=-lpthread -lm

UNAME := $(shell uname)
ifeq ($(UNAME),Linux)

#Conditional for Linux
CFLAGS+= $(shell pkg-config --cflags librtlsdr)
LDFLAGS+=$(shell pkg-config --libs librtlsdr)

else

#Conditional for Windows
#### point this to your correct path ###
RTLSDR_PATH="/c/tmp/rtl-sdr/"
RTLSDR_LIB=$(RTLSDR_PATH)/build/src/
########################################
CFLAGS+=-I $(RTLSDR_PATH)/include
LDFLAGS+=-L $(RTLSDR_LIB) -L/usr/lib -lusb-1.0 -lrtlsdr -lWs2_32

endif

CC?=gcc
SOURCES= \
	rtl_ais.c convenience.c \
	./aisdecoder/aisdecoder.c \
	./aisdecoder/sounddecoder.c \
	./aisdecoder/lib/receiver.c \
	./aisdecoder/lib/protodec.c \
	./aisdecoder/lib/hmalloc.c \
	./aisdecoder/lib/filter.c

OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=rtl_ais

all: $(SOURCES) $(EXECUTABLE)
    
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

.c.o:
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)
