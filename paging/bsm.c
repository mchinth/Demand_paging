/* bsm.c - manage the backing store mapping*/

#include <conf.h>
#include <kernel.h>
#include <paging.h>
#include <proc.h>
bs_map_t bsm_tab[16];
/*-------------------------------------------------------------------------
 * init_bsm- initialize bsm_tab
 *-------------------------------------------------------------------------
 */
SYSCALL init_bsm() {
	STATWORD ps;
    disable(ps);
	int i;
	for (i = 0; i < 16; i++) {
	
	bsm_tab[i].bs_npages = 0;
	bsm_tab[i].bs_pid = -1;
	bsm_tab[i].bs_sem = -1;
	bsm_tab[i].bs_status = BSM_UNMAPPED;
	bsm_tab[i].bs_vpno = -1;
	bsm_tab[i].bs_refCount = 0;
	bsm_tab[i].bs_PrivHeap = 0;
	}
	restore(ps);
    return OK;
}

/*-------------------------------------------------------------------------
 * get_bsm - get a free entry from bsm_tab 
 *-------------------------------------------------------------------------
 */
SYSCALL get_bsm(int *avail) {
	STATWORD ps;
    disable(ps);
	int i;
	for (i = 0; i < 16; i++) {
		if (bsm_tab[i].bs_status == BSM_UNMAPPED)
		{
			*avail = i;
			restore(ps);
			return OK;
		}
	}
	restore(ps);
	return SYSERR;
}

/*-------------------------------------------------------------------------
 * free_bsm - free an entry from bsm_tab 
 *-------------------------------------------------------------------------
 */
SYSCALL free_bsm(int i) {
	STATWORD ps;
	disable(ps);
	if(bsm_tab[i].bs_refCount !=0 || i <0 || i>15){
		restore(ps);
		return SYSERR;
	}
	bsm_tab[i].bs_status = BSM_UNMAPPED;
	bsm_tab[i].bs_npages = 0;
	bsm_tab[i].bs_pid = -1;
	bsm_tab[i].bs_refCount = 0;
	bsm_tab[i].bs_sem = -1;
	bsm_tab[i].bs_vpno = -1;
	bsm_tab[i].bs_PrivHeap =0;

	restore(ps);
	return OK;

}

/*-------------------------------------------------------------------------
 * bsm_lookup - lookup bsm_tab and find the corresponding entry
 *-------------------------------------------------------------------------
 */
SYSCALL bsm_lookup(int pid, long vaddr, int* store, int* pageth) {

	unsigned int i;
	unsigned int vpno = (vaddr / NBPG) & 0x000fffff;
	bs_map_t *ptr;
	STATWORD ps;
	disable(ps);
	for (i = 0; i < 16; i++) {
		ptr = &proctab[pid].BSMapping[i];
		if ((ptr->bs_status == BSM_MAPPED) && (ptr->bs_vpno <= vpno) && (ptr->bs_vpno + ptr->bs_npages > vpno)) {
			*store = i;
			*pageth = vpno - ptr->bs_vpno;
			restore(ps);
			return OK;
		}
	}
	*store = -1;
	*pageth = -1;
	return SYSERR;
}

/*-------------------------------------------------------------------------
 * bsm_map - add an mapping into bsm_tab 
 *-------------------------------------------------------------------------
 */
SYSCALL bsm_map(int pid, int vpno, int source, int npages) {
	STATWORD ps;
	disable(ps);
	if (source<0 ||source> 15||npages< 0 || npages > 127 ||vpno < NBPG ){
		restore(ps);
		return SYSERR;
	}
	if (bsm_tab[source].bs_status == UNMAPPED) {
		bsm_tab[source].bs_status = MAPPED;
		bsm_tab[source].bs_npages = npages;
		return OK;
	}
	else {
		if (npages > bsm_tab[source].bs_npages)
		{
			restore(ps);
			return SYSERR;
		}
	}
	   if (proctab[pid].BSMapping[source].bs_status == BSM_UNMAPPED)
                bsm_tab[source].bs_refCount++;

	proctab[pid].BSMapping[source].bs_PrivHeap = 0;
	proctab[pid].BSMapping[source].bs_npages = npages;
	proctab[pid].BSMapping[source].bs_pid = pid;
	proctab[pid].BSMapping[source].bs_refCount = 0;
	proctab[pid].BSMapping[source].bs_sem = -1;
	proctab[pid].BSMapping[source].bs_status = BSM_MAPPED;
	proctab[pid].BSMapping[source].bs_vpno = vpno;
	            	
/*	if (proctab[pid].BSMapping[source].bs_status == BSM_UNMAPPED)
		bsm_tab[source].bs_refCount++;
*/	
	restore(ps);
	return OK;

}

/*-------------------------------------------------------------------------
 * bsm_unmap - delete an mapping from bsm_tab
 *-------------------------------------------------------------------------
 */
SYSCALL bsm_unmap(int pid, int vpno, int flag) {

	if (vpno < NBPG) {
		return SYSERR;
	}

	int store, pageth;
	long  vaddr = vpno * NBPG;
 
	if ((bsm_lookup(pid, vaddr, &store, &pageth)== SYSERR) || (store == -1) || (pageth == -1)) {
		return SYSERR;
	}

	proctab[pid].BSMapping[store].bs_PrivHeap = 0;
	proctab[pid].BSMapping[store].bs_npages = 0;
	proctab[pid].BSMapping[store].bs_pid = -1;
	proctab[pid].BSMapping[store].bs_sem = -1;
	proctab[pid].BSMapping[store].bs_status = BSM_UNMAPPED;
	proctab[pid].BSMapping[store].bs_vpno = -1;
	proctab[pid].BSMapping[store].bs_refCount = 0;
	
	if (flag == PRIVATE_HEAP) {
		proctab[pid].vmemlist = NULL;
	} else {
		//int virt_n = vpno;

		virt_addr_t  *v_addr = vpno *NBPG;
			while (proctab[pid].BSMapping[store].bs_npages + proctab[pid].BSMapping[store].bs_vpno > vpno) {
			pd_t *pd_entry= proctab[pid].pdbr + (v_addr->pd_offset) * sizeof(pd_t);
						if (pd_entry->pd_pres == 0)
				break;
			pt_t *pt_entry= (pd_entry->pd_base) * NBPG + sizeof(pt_t) * (v_addr->pt_offset );

			if (frm_tab[pt_entry->pt_base - FRAME0].fr_status == MAPPED)
				free_frm(pt_entry->pt_base - FRAME0);

			vpno++;
		}
	}


	bsm_tab[store].bs_refCount--;
	return OK;
}


