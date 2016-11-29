/*
 * DiskSim Storage Subsystem Simulation Environment (Version 4.0)
 * Revision Authors: John Bucy, Greg Ganger
 * Contributors: John Griffin, Jiri Schindler, Steve Schlosser
 *
 * Copyright (c) of Carnegie Mellon University, 2001-2008.
 *
 * This software is being provided by the copyright holders under the
 * following license. By obtaining, using and/or copying this software,
 * you agree that you have read, understood, and will comply with the
 * following terms and conditions:
 *
 * Permission to reproduce, use, and prepare derivative works of this
 * software is granted provided the copyright and "No Warranty" statements
 * are included with all reproductions and derivative works and associated
 * documentation. This software may also be redistributed without charge
 * provided that the copyright and "No Warranty" statements are included
 * in all redistributions.
 *
 * NO WARRANTY. THIS SOFTWARE IS FURNISHED ON AN "AS IS" BASIS.
 * CARNEGIE MELLON UNIVERSITY MAKES NO WARRANTIES OF ANY KIND, EITHER
 * EXPRESSED OR IMPLIED AS TO THE MATTER INCLUDING, BUT NOT LIMITED
 * TO: WARRANTY OF FITNESS FOR PURPOSE OR MERCHANTABILITY, EXCLUSIVITY
 * OF RESULTS OR RESULTS OBTAINED FROM USE OF THIS SOFTWARE. CARNEGIE
 * MELLON UNIVERSITY DOES NOT MAKE ANY WARRANTY OF ANY KIND WITH RESPECT
 * TO FREEDOM FROM PATENT, TRADEMARK, OR COPYRIGHT INFRINGEMENT.
 * COPYRIGHT HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE
 * OR DOCUMENTATION.
 *
 */



/*
 * DiskSim Storage Subsystem Simulation Environment (Version 2.0)
 * Revision Authors: Greg Ganger
 * Contributors: Ross Cohen, John Griffin, Steve Schlosser
 *
 * Copyright (c) of Carnegie Mellon University, 1999.
 *
 * Permission to reproduce, use, and prepare derivative works of
 * this software for internal use is granted provided the copyright
 * and "No Warranty" statements are included with all reproductions
 * and derivative works. This software may also be redistributed
 * without charge provided that the copyright and "No Warranty"
 * statements are included in all redistributions.
 *
 * NO WARRANTY. THIS SOFTWARE IS FURNISHED ON AN "AS IS" BASIS.
 * CARNEGIE MELLON UNIVERSITY MAKES NO WARRANTIES OF ANY KIND, EITHER
 * EXPRESSED OR IMPLIED AS TO THE MATTER INCLUDING, BUT NOT LIMITED
 * TO: WARRANTY OF FITNESS FOR PURPOSE OR MERCHANTABILITY, EXCLUSIVITY
 * OF RESULTS OR RESULTS OBTAINED FROM USE OF THIS SOFTWARE. CARNEGIE
 * MELLON UNIVERSITY DOES NOT MAKE ANY WARRANTY OF ANY KIND WITH RESPECT
 * TO FREEDOM FROM PATENT, TRADEMARK, OR COPYRIGHT INFRINGEMENT.
 */


#include "disksim_global.h"
#include "disksim_malloc.h"

/* GROK -- temporary, while this is in progress */
#include <stdlib.h>

void *DISKSIM_malloc (int size)
{
	/*     void *addr; */

	/*     if (disksim == NULL) { */
	/*        fprintf (stderr, "DISKSIM_malloc: disksim structure not yet initialized\n"); */
	/*        exit(1); */
	/*     } */

	/*     addr = disksim->startaddr + disksim->curroffset; */

	/*     if ((disksim->totallength - disksim->curroffset) >= size) { */
	/*        disksim->curroffset += rounduptomult(size,8); */
	/*     }  */
	/*     else { */
	/*       fprintf (stderr, "*** error: DISKSIM_malloc(): allocated space for disksim run has run out (%d)\n", disksim->totallength); */
	/*        exit(1); */
	/*     } */

	/*     return (addr); */
	return malloc(size);

}

#include "disksim_container.h"
#include "disksim_erasure.h"
#include "disksim_event_queue.h"
#include "disksim_interface.h"
#include "disksim_req.h"

#define INDEX(type) malloc_index(sizeof(struct type))

int hn_idx; // hash_node & release in ht_remove (CK)
int en_idx; // event_node & release in main (CK)
int sh_idx; // stripe_head & DO NOT RELEASE (CK)
int wt_idx; // wait_req & release in sh_release_stripe (CK)
int dr_idx; // disksim_request & release in main (CK)
int el_idx; // element & DO NOT RELEASE
int rq_idx; // ioreq & release in main (CK)
int sb_idx; // sub_ioreq & release in maprequest_iocomplete (CK)

static void* space[32];

int malloc_index(unsigned x)
{
    int n = 2; x >>= 2;
    if (x >> 8) {x >>= 8; n += 8;}
    if (x >> 4) {x >>= 4; n += 4;}
    if (x >> 2) {x >>= 2; n += 2;}
    if (x >> 1) {x >>= 1; n += 1;}
    return n;
}

void malloc_initialize()
{
	hn_idx = INDEX(hash_node);
	en_idx = INDEX(event_node);
	sh_idx = INDEX(stripe_head);
	wt_idx = INDEX(wait_request);
	dr_idx = INDEX(disksim_request);
	el_idx = INDEX(element);
	rq_idx = INDEX(ioreq);
	sb_idx = INDEX(sub_ioreq);
}

void* disksim_malloc(int index)
{
	void **ptr = &space[index];
	int size = (4 << index);
	if (*ptr == NULL) {
		void *tmp = (void*) malloc(size << 4); // 16 elements
		void *addr = tmp, *end = tmp + (size << 4);
		while (addr < end) {
			*(void**)addr = *ptr;
			*ptr = addr;
			addr += size;
		}
	}
	void *ret = *ptr;
	*ptr = *(void**)(*ptr);
	return ret;
}

void disksim_free(int index, void *item)
{
	void **ptr = &space[index];
	*(void**)item = *ptr;
	*ptr = item;
}
