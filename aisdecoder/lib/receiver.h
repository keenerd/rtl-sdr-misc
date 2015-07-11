
/*
 *	receiver.h
 *
 *	(c) Ruben Undheim 2008
 *	(c) Heikki Hannikainen 2008
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


#ifndef INC_RECEIVER_H
#define INC_RECEIVER_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include "protodec.h"
#include "callbacks.h"

struct receiver {
	struct filter *filter;
	char name;
	int lastbit;
	int num_ch;
	int ch_ofs;
	unsigned int pll;
	unsigned int pllinc;
	struct demod_state_t *decoder;
	int prev;
	time_t last_levellog;
};

extern struct receiver *init_receiver(char name, int num_ch, int ch_ofs);
extern void free_receiver(struct receiver *rx);

extern void receiver_run(struct receiver *rx, short *buf, int len);

#ifdef __cplusplus
}
#endif
#endif
