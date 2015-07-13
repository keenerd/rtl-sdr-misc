/*
 *    sounddecoder.cpp
 *
 *    This file is part of AISDecoder.
 *
 *    Copyright (C) 2013
 *      Astra Paging Ltd / AISHub (info@aishub.net)
 *
 *    AISDecoder is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    AISDecoder uses parts of GNUAIS project (http://gnuais.sourceforge.net/)
 *
 */

#include <string.h>
#include <stdio.h>
//#include "config.h"
#ifdef WIN32
	#include <fcntl.h>
#endif

#include "receiver.h"
#include "hmalloc.h"

#define MAX_FILENAME_SIZE 512
#define ERROR_MESSAGE_LENGTH 1024
#include "sounddecoder.h"


char errorSoundDecoder[ERROR_MESSAGE_LENGTH];

static struct receiver *rx_a=NULL;
static struct receiver *rx_b=NULL;

static short *buffer=NULL;
static int buffer_l=0;
static int buffer_read=0;
static int channels=0;
static Sound_Channels sound_channels;
static FILE *fp=NULL;
static void readBuffers();
static time_t tprev=0;
static int time_print_stats=0;

int initSoundDecoder(int buf_len,int _time_print_stats) 
{
	sound_channels=SOUND_CHANNELS_STEREO;
	channels = sound_channels == SOUND_CHANNELS_MONO ? 1 : 2;
	time_print_stats=_time_print_stats;
	tprev=time(NULL); // for decoder statistics
    buffer = (short *) hmalloc(channels*sizeof(short)*buf_len);
    rx_a = init_receiver('A', 2, 0);
    rx_b = init_receiver('B', 2, 1);
    return 1;
}

void run_mem_decoder(short * buf, int len,int max_buf_len)
{	
	int offset=0;
	int bytes_in_len=len*channels;
	char * p=(char *) buf;
	while(bytes_in_len > max_buf_len )
	{
		memcpy(buffer,p+offset,max_buf_len);
		buffer_read=max_buf_len/(channels*sizeof(short));
		bytes_in_len-=max_buf_len;
		offset+=max_buf_len;
		readBuffers();
	}
	memcpy(buffer,p+offset,bytes_in_len);
	buffer_read=bytes_in_len/(channels*sizeof(short));
	readBuffers();
	
	if(time_print_stats && (time(NULL)-tprev >= time_print_stats))
	{
		struct demod_state_t *d = rx_a->decoder;
		tprev=time(NULL);
		fprintf(stderr,
				"A: Received correctly: %d packets, wrong CRC: %d packets, wrong size: %d packets\n",
				d->receivedframes, d->lostframes,
				d->lostframes2);
		d = rx_b->decoder;
			fprintf(stderr,
				"B: Received correctly: %d packets, wrong CRC: %d packets, wrong size: %d packets\n",
				d->receivedframes, d->lostframes,
				d->lostframes2);
	}
}
void runSoundDecoder(int *stop) {
    while (!*stop) {
        buffer_read = fread(buffer, channels * sizeof(short), buffer_l, fp);
        readBuffers();
    }
}

static void readBuffers() {
    if (buffer_read <= 0) return;
    if (rx_a != NULL && sound_channels != SOUND_CHANNELS_RIGHT)
        receiver_run(rx_a, buffer, buffer_read);

    if (rx_b != NULL &&
        (sound_channels == SOUND_CHANNELS_STEREO || sound_channels == SOUND_CHANNELS_RIGHT)
    ) receiver_run(rx_b, buffer, buffer_read);
}

void freeSoundDecoder(void) {
    if (fp != NULL) {
        fclose(fp);
        fp=NULL;
    }

    if (rx_a != NULL) {
        free_receiver(rx_a);
        rx_a=NULL;
    }

    if (rx_b != NULL) {
        free_receiver(rx_b);
        rx_b=NULL;
    }
    if (buffer != NULL) {
        hfree(buffer);
        buffer = NULL;
    }
}
