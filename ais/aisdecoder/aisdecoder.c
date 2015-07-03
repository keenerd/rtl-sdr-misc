/*
 *    main.cpp  --  AIS Decoder
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
/* This is a stripped down version for use with rtl_ais*/ 

#ifndef WIN32
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#else
// Horrible hack for compiling freeaddrinfo() and getaddrinfo() with MSys, fix this please
#define WIN32_VER_TMP _WIN32_WINNT
#define _WIN32_WINNT 0x0502
#include <winsock2.h>
#include <ws2tcpip.h>
#undef   _WIN32_WINNT
#define _WIN32_WINNT WIN32_VER_TMP

#endif
#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
//#include "config.h"
#include "sounddecoder.h"
#include "callbacks.h"

#define MAX_BUFFER_LENGTH 2048
//#define MAX_BUFFER_LENGTH 8190

static char buffer[MAX_BUFFER_LENGTH];
static unsigned int buffer_count=0;
#ifdef WIN32
	WSADATA wsaData;
#endif
static int debug_nmea;
static int sock;
static struct addrinfo* addr=NULL;

static int initSocket(const char *host, const char *portname);


void sound_level_changed(float level, int channel, unsigned char high) {
    if (high != 0)
        fprintf(stderr, "Level on ch %d too high: %.0f %%\n", channel, level);
    else
        fprintf(stderr, "Level on ch %d: %.0f %%\n", channel, level);
}

void nmea_sentence_received(const char *sentence,
                          unsigned int length,
                          unsigned char sentences,
                          unsigned char sentencenum) {
    if (sentences == 1) {
        if (sendto(sock, sentence, length, 0, addr->ai_addr, addr->ai_addrlen) == -1) abort();
        if (debug_nmea) fprintf(stderr, "%s", sentence);
    } else {
        if (buffer_count + length < MAX_BUFFER_LENGTH) {
            memcpy(&buffer[buffer_count], sentence, length);
            buffer_count += length;
        } else {
            buffer_count=0;
        }

        if (sentences == sentencenum && buffer_count > 0) {
            if (sendto(sock, buffer, buffer_count, 0, addr->ai_addr, addr->ai_addrlen) == -1) abort();
            if (debug_nmea) fprintf(stderr, "%s", buffer);
            buffer_count=0;
        };
    }
}
int init_ais_decoder(char * host, char * port ,int show_levels,int _debug_nmea,int buf_len,int time_print_stats){
	debug_nmea=_debug_nmea;
	if(debug_nmea)
		fprintf(stderr,"Log NMEA sentences to console ON\n");
	else
		fprintf(stderr,"Log NMEA sentences to console OFF\n");
		
    if (!initSocket(host, port)) {
        return EXIT_FAILURE;
    }
    if (show_levels) on_sound_level_changed=sound_level_changed;
    on_nmea_sentence_received=nmea_sentence_received;
	initSoundDecoder(buf_len,time_print_stats); 
	return 0;
}	

void run_rtlais_decoder(short * buff, int len)
{
	run_mem_decoder(buff,len,MAX_BUFFER_LENGTH);
}
int free_ais_decoder(void)
{
    freeSoundDecoder();
    freeaddrinfo(addr);
#ifdef WIN32
    WSACleanup();
#endif
    return 0;
}


int initSocket(const char *host, const char *portname) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family=AF_UNSPEC;
    hints.ai_socktype=SOCK_DGRAM;
    hints.ai_protocol=IPPROTO_UDP;
#ifndef WIN32
    hints.ai_flags=AI_ADDRCONFIG;
#else

    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed: %d\n", iResult);
        return 0;
    }
#endif
    int err=getaddrinfo(host, portname, &hints, &addr);
    if (err!=0) {
        fprintf(stderr, "Failed to resolve remote socket address!\n");
#ifdef WIN32
        WSACleanup();
#endif
        return 0;
    }

    sock=socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if (sock==-1) {
        fprintf(stderr, "%s",strerror(errno));
#ifdef WIN32
        WSACleanup();
#endif
        return 0;
    }
	fprintf(stderr,"AIS data will be sent to %s port %s\n",host,portname);
    return 1;
}

