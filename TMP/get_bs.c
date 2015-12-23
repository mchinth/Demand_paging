#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <paging.h>

int get_bs(bsd_t store, unsigned int npages) {

	STATWORD ps;
	
if(store >15  || store <0 ||npages <=0 ||npages >128)
{
		return SYSERR;
	}

	disable(ps);
	if (bsm_tab[store].bs_status == MAPPED) {
	if(bsm_tab[store].bs_PrivHeap == PRIVATE_HEAP)
	{

		restore(ps);
		return SYSERR;
	}
		if(npages >  bsm_tab[store].bs_npages)
		{
		 restore(ps);
                return SYSERR;
		}
	restore(ps);	
	return bsm_tab[store].bs_npages;
	}

	if (bsm_tab[store].bs_status == UNMAPPED) {

		if (bsm_map(currpid, 4096, store, npages) == SYSERR) {
			restore(ps);
			return SYSERR;
		}
		restore(ps);
		return npages;
	}
}

