/*
 *    filter.h  --  FIR filter
 *
 *    Copyright (C) 2001, 2002, 2003, 2004
 *      Tomi Manninen (oh2bns@sral.fi)
 *
 *    This file is part of gMFSK.
 *
 *    gMFSK is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    gMFSK is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with gMFSK; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _FILTER_H
#define _FILTER_H

#define BufferLen	1024

/* ---------------------------------------------------------------------- */

#ifdef __OPTIMIZE__

#ifdef __i386__
#include "filter-i386.h"
#endif				/* __i386__ */


#ifndef __HAVE_ARCH_MAC
static __inline__ float filter_mac(const float *a, const float *b, int size)
{
	float sum = 0;
	int i;

	for (i = 0; i < size; i++)
		sum += a[i] * b[i];
		
	return sum;
}
#endif				/* __HAVE_ARCH_MAC */

#endif				/* __OPTIMIZE__ */


/* ---------------------------------------------------------------------- */

struct filter {
	int length;
	float *taps;
	float buffer[BufferLen];
	int pointer;
};

extern struct filter *filter_init(int len, float *taps);
extern void filter_free(struct filter *f);

extern void filter_run(struct filter *f, float in, float *out);
extern short filter_run_buf(struct filter *f, short *in, float *out, int step, int len);

/* ---------------------------------------------------------------------- */

#endif				/* _FILTER_H */
