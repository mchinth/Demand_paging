/* xm.c = xmmap xmunmap */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <paging.h>

/*-------------------------------------------------------------------------
 * xmmap - xmmap
 *-------------------------------------------------------------------------
 */
SYSCALL xmmap(int virtpage, bsd_t source, int npages) {
	/* sanity check ! */
	STATWORD ps;
	if ((virtpage < 4096) || (source < 0) || (source > MAX_ID) || (npages < 1) || (npages > 128)) {
		kprintf("\n xmmap: invalid parameters\n");
		return SYSERR;
	}

	disable(ps);

	bs_map_t *bs= &bsm_tab[source];
	if ((bs->bs_status == UNMAPPED) || (bs->bs_PrivHeap == PRIVATE_HEAP)) {
		restore(ps);
		return OK;
	}
//	kprintf("\n mapping bsm_tab[%d].bs_npages = %d and npages = %d ", source , bsm_tab[source].bs_npages , npages);
	if (npages > bs->bs_npages) {
		restore(ps);
//		kprintf("\n SYSERR since npages > bsm_tab pages ");
		return SYSERR;
	}

	if(bsm_map(currpid, virtpage, source, npages) == SYSERR) {
//		kprintf("\n SYSERR , npages < bsm_tab pages but could not bsm_map");	
		restore(ps);
		return SYSERR;
	}
	restore(ps);
	return OK;
}

/*-------------------------------------------------------------------------
 * xmunmap - xmunmap
 *-------------------------------------------------------------------------
 */
SYSCALL xmunmap(int virtpage) {
	/* sanity check ! */
	STATWORD ps;
	int i=0;
	if ((virtpage < 4096)) {
		kprintf("xmunmap: %d invalid virtual page\n", virtpage);
		return SYSERR;
	}
	for (i=0;i<2;i++);
	disable(ps);
	if (bsm_unmap(currpid, virtpage, 0) == SYSERR) {
		restore(ps);
		return SYSERR;
	}
	
	write2CR3(currpid);
	restore(ps);
	return OK;
}
