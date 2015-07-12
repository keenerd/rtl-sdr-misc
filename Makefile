CFLAGS?=-O2 -g -Wall -W $(shell pkg-config --cflags librtlsdr)
CFLAGS+= -I./aisdecoder -I ./aisdecoder/lib
LDFLAGS+=$(shell pkg-config --libs librtlsdr) -lpthread -lm
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
