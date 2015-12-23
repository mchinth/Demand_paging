/* vfreemem.c - vfreemem */

#include <conf.h>
#include <kernel.h>
#include <mem.h>
#include <proc.h>
#include <paging.h>

extern struct pentry proctab[];
/*------------------------------------------------------------------------
 *  vfreemem  --  free a virtual memory block, returning it to vmemlist
 *------------------------------------------------------------------------
 */
SYSCALL vfreemem(struct mblock *block, unsigned int size) {
	/*
	 * Referred freemem.c
	 */

	STATWORD ps;

	disable(ps);

	if (size == 0 || block < 4096 * NBPG)
	{
	restore(ps);
	return SYSERR;
	}
 	int j;
	size = roundmb(size);

	struct pentry *pptr = &proctab[currpid];
	
	struct mblock *vmemlist1 = &pptr->vmemlist;
        struct mblock *next = vmemlist1->mnext;
	struct mblock *prev = vmemlist1;
	for(j=0;j<4;j++);
	while (next != NULL && next < block) {
		prev = next;
		next = next->mnext;
	}

	block->mnext = next;
	block->mlen = size;
	prev->mnext = block;
	
	
	restore(ps);
	return (OK);
}
