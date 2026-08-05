/*
 *	(c) Heikki Hannikainen, OH7LZB <hessu@hes.iki.fi>
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
 *
 */

/*
 *	Replacements for malloc, realloc and free, which never fail,
 *	and might keep statistics on memory allocation...
 *
 *	GPL'ed, by Heikki Hannikainen <hessu@hes.iki.fi>
 */

#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "hmalloc.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

int mem_panic = 0;

void *hmalloc(size_t size) {
    void *p;
    if (!(p = malloc(size))) {
        if (mem_panic) exit(1);
        mem_panic = 1;
        exit(1);
    }

    return p;
}

void *hrealloc(void *ptr, size_t size) {
    void *p;

    if (!(p = realloc(ptr, size))) {
        if (mem_panic) exit(1);
        mem_panic = 1;
        exit(1);
    }

    return p;
}

void hfree(void *ptr) {
    if (ptr) free(ptr);
}

char *hstrdup(const char *s) {
    char *p;

    p = (char*)hmalloc(strlen(s)+1);
    strcpy(p, s);

    return p;
}

