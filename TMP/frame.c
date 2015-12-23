/* avail_frm.c - manage physical frames */
#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <paging.h>
fr_map_t frm_tab[NFRAMES];
//void clearPageTableEntry(pt_t *pt_entry);
void clear_FrameTable(pd_t *pd_entry, int i, int PAGE);
//void frm_invalidate_TLB(int frm_num);
int write2CR3(int pid);
int GPT[4];
/*-------------------------------------------------------------------------
 * init_frm - initialize frm_tab
 *-------------------------------------------------------------------------
 */
SYSCALL init_frm() {
	int i;
	for (i = 0; i < NFRAMES; i++) {
		frm_tab[i].fr_dirty = 0;
		frm_tab[i].fr_loadtime = -1;
		frm_tab[i].fr_pid = -1;
		frm_tab[i].fr_status = FRM_UNMAPPED;
		frm_tab[i].fr_refcnt = 0;
		frm_tab[i].fr_type = -1;
		frm_tab[i].fr_vpno = -1;
	}
	return OK;
}

/*-------------------------------------------------------------------------
 * get_frm - get a free avail_frm according page replacement policy
 *-------------------------------------------------------------------------
 */
SYSCALL get_frm(int *avail) {
	int i,avail_frm;
int page_replace_policy;
	STATWORD ps;
	disable(ps);
	for (i = 0; i < NFRAMES; i++) {
		if (frm_tab[i].fr_status == UNMAPPED) {
			 *avail = i;
	//kprintf("\n %d avail_frm ",*avail);
			restore(ps);
			return i;
		}
	}
//	kprintf("Page Replacement Policy %d\n",grpolicy());
	page_replace_policy = grpolicy();
	if (page_replace_policy == FIFO) {
		     int j;
        fifo_q *current_frm = &fifo_hd;
        for (j = 0; j < Fifo_npages; j++);
        while( j>0)
        {
                current_frm = current_frm->next;
                j--;
        }
        //freemem(current, sizeof(fifo_q));
        if (Fifo_npages > 0)
                Fifo_npages--;
        free_frm(current_frm->frameID + FRAME0);
        avail_frm = current_frm->frameID;
        //kprintf("Frame %d is replaced.\n", avail_frm);
        freemem(current_frm, sizeof(fifo_q));


	
		*avail = avail_frm;
		restore(ps);
		return avail_frm;
	}
	if (page_replace_policy == LRU) {
		avail_frm = get_LRU_frame();
		if (avail_frm == SYSERR)
{
restore(ps);
			return SYSERR;
}
		free_frm(avail_frm + FRAME0);
		*avail = avail_frm;
restore(ps);
		return avail_frm;
	}
	return -1;
}

/*-------------------------------------------------------------------------
 * free_frm - free a avail_frm
 *-------------------------------------------------------------------------
 */
SYSCALL free_frm(int i) {

	if (i <0|| i> 1024) {
		return SYSERR;
	}
	int pid =frm_tab[i].fr_pid;
int vpno =frm_tab[i].fr_vpno * NBPG;	
	if (frm_tab[i].fr_type==FR_PAGE) {
		int store, pageth;
		if ((bsm_lookup(pid, vpno, &store, &pageth) == SYSERR) || (store == -1) || (pageth == -1)) {
			return SYSERR;
		}
		write_bs((i + FRAME0) * NBPG, store, pageth);
		pd_t *pd_entry = proctab[pid].pdbr + (vpno >>12) * sizeof(pd_t);
		pt_t *pt_entry = (pd_entry->pd_base) * NBPG + (vpno % 1024) * sizeof(pt_t);
		if (pt_entry->pt_pres == 0) {
			return SYSERR;
		}
	 pt_entry->pt_acc = 0;
        pt_entry->pt_avail = 0;
        pt_entry->pt_base = 0;
        pt_entry->pt_dirty = 0;
        pt_entry->pt_global = 0;
        pt_entry->pt_mbz = 0;
        pt_entry->pt_pcd = 0;
        pt_entry->pt_pres = 0;
        pt_entry->pt_pwt = 0;
        pt_entry->pt_write = 1;
        pt_entry->pt_user = 0;

		clear_FrameTable(pd_entry, i, 1);

		return OK;
	}
	else if(frm_tab[i].fr_type== FR_DIR) {
		int j;
		for (j = 4; j < 1024; j++) {
			pd_t *pd_entry = proctab[pid].pdbr + j * sizeof(pd_t);
			if (pd_entry->pd_pres == 1) {
				free_frm(pd_entry->pd_base - FRAME0);
			}
		}
		clear_FrameTable(NULL, i, 0);
		return OK;
	}
	
	return SYSERR;
}
void clear_FrameTable(pd_t *pd_entry, int i, int PAGE) {
	frm_tab[i].fr_dirty = CLEAN;
	frm_tab[i].fr_loadtime = -1;
	frm_tab[i].fr_pid = -1;
	frm_tab[i].fr_refcnt = 0;
	frm_tab[i].fr_status = UNMAPPED;
	frm_tab[i].fr_type = -1;
	frm_tab[i].fr_vpno = -1;

	if (0==PAGE) {
		frm_tab[pd_entry->pd_base - FRAME0].fr_refcnt--;
		if (frm_tab[pd_entry->pd_base - FRAME0].fr_refcnt <= 0) {
			free_frm(pd_entry->pd_base - FRAME0);
		}
	}

}

int get_LRU_frame() {
	STATWORD ps;
	unsigned long int min = 429496729;
	int i;
	int retFrame = 0;
	fr_map_t *tmp_frm;

	for (i = 0; i < NFRAMES; i++) {
		tmp_frm = &frm_tab[i];
		if (tmp_frm->fr_type == FR_PAGE) {
			if (tmp_frm->fr_loadtime < min) {
				min = tmp_frm->fr_loadtime;
				retFrame = i;
			}

			else if (tmp_frm->fr_loadtime == min) {
				if (frm_tab[retFrame].fr_vpno < tmp_frm->fr_vpno)
					retFrame = i;
			}
		}
	}
	restore(ps);
	kprintf("%d avail_frm is replaced\n",retFrame);
	return retFrame;
}

int LRU_updateTimeCount() {
/*	STATWORD ps;
	int i, j, pid;
	struct pentry *pptr;
	unsigned long pdbr;
	pd_t *pageDir_entry;
	pt_t *pageTable_entry;
	int avail_frm;

	disable(ps);
	LRU_Count++;

	for (pid = 1; pid < NPROC; pid++) {
		pptr = &proctab[pid];
		if (pptr->pstate != PRFREE) {
			pdbr = pptr->pdbr;
			pageDir_entry = pdbr;

			for (i = 0; i < 1024; i++) {
				if (pageDir_entry->pd_pres == 1) {
					pageTable_entry = (pageDir_entry->pd_base) * NBPG;
					for (j = 0; j < 1024; j++) {
						if (pageTable_entry->pt_pres == 1 && pageTable_entry->pt_acc == 1) {
							avail_frm = pageTable_entry->pt_base - FRAME0;
							pageTable_entry->pt_acc = 0;
							frm_tab[avail_frm].fr_loadtime = LRU_Count;
						}
						pageTable_entry++;
					}
				}
				pageDir_entry++;
			}
		}
	}

	restore(ps);
	return OK;*/
}

void insert_Frame_FIFO(int i) {

//	kprintf("Inserted page in FIFO Queue as FIFO is page replacement policy %d\n", i);
	fifo_q *FIFO_tmp = (fifo_q*) getmem(sizeof(fifo_q));
	FIFO_tmp->frameID = i;
	FIFO_tmp->next = fifo_hd.next;
	fifo_hd.next = FIFO_tmp;
	Fifo_npages++;
}

int create_PageTable(int pid) {

	int i, avail_frm =0;
	int frm = -1;
	avail_frm = get_frm(&frm);
	if (avail_frm == -1) {
		return SYSERR;
	}

	frm_tab[avail_frm].fr_refcnt = 0;
	frm_tab[avail_frm].fr_type = FR_TBL;
	frm_tab[avail_frm].fr_dirty = CLEAN;
	frm_tab[avail_frm].fr_loadtime = -1;
	frm_tab[avail_frm].fr_status = MAPPED;
	frm_tab[avail_frm].fr_pid = pid;
	frm_tab[avail_frm].fr_vpno = -1;

	pt_t *pt_entry = (FRAME0 + avail_frm) * NBPG ;
	for (i = 0; i < 1024; i++) {
		//pt_t *pt = (FRAME0 + avail_frm) * NBPG + i * sizeof(pt_t);
		pt_entry[i].pt_dirty = CLEAN;
		 pt_entry[i].pt_mbz = 0;
		 pt_entry[i].pt_global = 0;
		pt_entry[i].pt_avail = 0;
		pt_entry[i].pt_base = 0;
		pt_entry[i].pt_pres = 0;
		pt_entry[i].pt_write = 1;
		pt_entry[i].pt_user = 0;
		pt_entry[i].pt_pwt = 0;
		pt_entry[i].pt_pcd = 0;
		pt_entry[i].pt_acc = 0;
	}
	return avail_frm;
}
/*
int create_PageDirectory(int pid) {
	int i, avail_frm=0;;
	int frm =-1;
	 avail_frm=get_frm(&frm);

	if (avail_frm == -1) {
		return -1;
	}
	frm_tab[avail_frm].fr_dirty = CLEAN;
	frm_tab[avail_frm].fr_loadtime = -1;
	frm_tab[avail_frm].fr_pid = pid;
	frm_tab[avail_frm].fr_refcnt = 4;
	frm_tab[avail_frm].fr_status = MAPPED;
	frm_tab[avail_frm].fr_type = FR_DIR;
	frm_tab[avail_frm].fr_vpno = -1;

	proctab[pid].pdbr = (FRAME0 + avail_frm) * NBPG;
	for (i = 0; i < 4; i++) {
                pd_t *pd_entry = proctab[pid].pdbr + (i * sizeof(pd_t));
                pd_entry->pd_pcd = 0;
                pd_entry->pd_acc = 0;
                pd_entry->pd_mbz = 0;
                pd_entry->pd_fmb = 0;
                pd_entry->pd_global = 0;
                pd_entry->pd_avail = 0;
                pd_entry->pd_base = 0;
                pd_entry->pd_pres = 1;
                pd_entry->pd_write = 1;
                pd_entry->pd_user = 0;
                pd_entry->pd_pwt = 0;
	pd_entry->pd_base = GPT[i];
}

	for (i = 4; i < 1024; i++) {
		pd_t *pd_entry = proctab[pid].pdbr + (i * sizeof(pd_t));
		pd_entry->pd_pcd = 0;
		pd_entry->pd_acc = 0;
		pd_entry->pd_mbz = 0;
		pd_entry->pd_fmb = 0;
		pd_entry->pd_global = 0;
		pd_entry->pd_avail = 0;
		pd_entry->pd_base = 0;
		pd_entry->pd_pres = 0;
		pd_entry->pd_write = 1;
		pd_entry->pd_user = 0;
		pd_entry->pd_pwt = 0;

		if (i < 4) {
			pd_entry->pd_pres = 1;
			pd_entry->pd_write = 1;
			pd_entry->pd_base = GPT[i];
		}
	}
	return avail_frm;
}

int initializeGlobalPageTable() {
	int i, j, k;
	for (i = 0; i < 4; i++) {
		k = create_PageTable(NULLPROC);
		if (k == -1) {
			return SYSERR;
		}
		GPT[i] = FRAME0 + k;

		for (j = 0; j < 1024; j++) {
			pt_t *pt = GPT[i] * NBPG + j * sizeof(pt_t);

			pt->pt_pres = 1;
			pt->pt_write = 1;
			pt->pt_base = j + i * 1024;

			frm_tab[k].fr_refcnt++;
		}
	}
	return OK;
}
*/
int write2CR3(int pid) {
	unsigned int pdbr = (proctab[pid].pdbr) / NBPG;
	 //kprintf("entered in write2CR3");
	if ((frm_tab[pdbr - FRAME0].fr_status != MAPPED) || (frm_tab[pdbr - FRAME0].fr_type != FR_DIR) || (frm_tab[pdbr - FRAME0].fr_pid != pid)) {
 //kprintf("failed in write2CR3");

		return SYSERR;
	}
	write_cr3(proctab[pid].pdbr);
	return OK;
}
