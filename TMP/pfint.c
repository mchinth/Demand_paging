/* pfint.c - pfint */

#include <conf.h>
#include <kernel.h>
#include <paging.h>
#include <proc.h>

/*-------------------------------------------------------------------------
 * pfint - paging fault ISR
 *-------------------------------------------------------------------------
 */
SYSCALL pfint() {

	STATWORD ps;
	disable(ps);

	int store, pageth;
	unsigned long a = read_cr2();
	pd_t *pd = proctab[currpid].pdbr + sizeof(pd_t) * (a>>22); //current page directory
	//kprintf("\n Pointing to current page directory [pdbr + offset] : %x " , pd);
	if ((bsm_lookup(currpid, a, &store, &pageth) == SYSERR) || (store == -1) || (pageth == -1)) {
		kill(currpid);
		restore(ps);
		return (SYSERR);
	}

	//kprintf("\n Page directory  present bit value : %d ",pd->pd_pres);
	if (pd->pd_pres == 0) {
		int new_frame = create_PageTable(currpid);
		//kprintf("\n No page table entry present , obtaining new frame %d " , new_frame);
		if (new_frame == -1) {
			//kprintf("pfint: Cannot create page table\n");
			return SYSERR;
		}
		pd->pd_pres = 1;
		pd->pd_write = 1;
		pd->pd_base = new_frame + FRAME0;
		//kprintf("\n enter address of new frame in pd_base %d ", pd->pd_base );
		
		
		frm_tab[(unsigned int) pd / NBPG - FRAME0].fr_refcnt++;
		//kprintf("\n inserting the assigned frame refcount %d " , (unsigned int) pd / NBPG - FRAME0);
	}
        pt_t *pageTableEntry = (pd->pd_base) * NBPG + sizeof(pt_t) * ((a >> 12) & 0x000003ff);
	//kprintf("\n Page table present bit value : %d ",pageTableEntry->pt_pres);
	int frame;
	frame = get_frm();
	if (frame == -1) {
		//kprintf("pfint: failed to get new frame .\n");
		kill(currpid);
		restore(ps);
		return SYSERR;
	}
	//frame = Obtain a free frame
	frm_tab[frame].fr_dirty = CLEAN;
	frm_tab[frame].fr_loadtime = -1;
	frm_tab[frame].fr_pid = currpid;
	frm_tab[frame].fr_refcnt = 1;
	frm_tab[frame].fr_status = FRM_MAPPED;
	frm_tab[frame].fr_type = FR_PAGE;
	frm_tab[frame].fr_vpno = (a / NBPG) & 0x000fffff;
	//kprintf(" \n Frame %d pointing to  vpno = %x " , frame , frm_tab[frame].fr_vpno );
	//Copying page 'pageth' of store 'store' to frame.
	//kprintf("\n now reading the value from the frame %x\n",(frame + FRAME0) * NBPG);
	read_bs((frame + FRAME0) * NBPG, store, pageth);
	
	
	if (page_replace_policy == FIFO)
		insert_Frame_FIFO(frame);
	else if (page_replace_policy == LRU) {
		LRU_updateTimeCount();
	}
	
	//pt_t *pageTableEntry = (pd->pd_base) * NBPG + sizeof(pt_t) * (PT_OFFSET(a));
	pageTableEntry->pt_pres = 1;
	pageTableEntry->pt_write = 1;
	pageTableEntry->pt_base = frame + FRAME0;
	//kprintf("\n pt_base %d", pageTableEntry->pt_base); 
	frm_tab[(unsigned int) pageTableEntry / NBPG - FRAME0].fr_refcnt++;
	//kprintf("\n writing into the page ");	
	write2CR3(currpid);
	restore(ps);
	return OK;
}

