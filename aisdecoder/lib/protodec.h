
/*
 *	protodec.h
 *
 *	(c) Ruben Undheim 2008
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef INC_PROTODEC_H
#define INC_PROTODEC_H

#define ST_SKURR 1
#define ST_PREAMBLE 2
#define ST_STARTSIGN 3
#define ST_DATA 4
#define ST_STOPSIGN 5

#define DEMOD_BUFFER_LEN 450
#define MAX_AIS_PACKET_TYPE 27
#define NMEABUFFER_LEN 100

struct demod_state_t {
	char chanid;
	int state;
	unsigned int offset;
	int nskurr, npreamble, nstartsign, ndata, nstopsign;
	
	int antallenner;
	unsigned char *buffer;
	unsigned char *rbuffer;
	char *tbuffer;
	int bufferpos;
	char last;
	int antallpreamble;
	int bitstuff;
	int receivedframes;
	int lostframes;
	int lostframes2;
	unsigned char seqnr;
	
	struct serial_state_t *serial;
	
	char *nmea;
};

void protodec_initialize(struct demod_state_t *d, struct serial_state_t *serial, char chanid);
void protodec_reset(struct demod_state_t *d);
void protodec_getdata(int bufferlengde, struct demod_state_t *d);
void protodec_decode(char *in, int count, struct demod_state_t *d);

#endif
