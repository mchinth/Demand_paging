#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <paging.h>

SYSCALL release_bs(bsd_t bs_id) {

	if (bs_id <0 ||bs_id > 15) {
//kprintf("syserr  with backind store number ");
		return SYSERR;
	}
//	kprintf("\n Starting to release the Backing store %d ",bs_id);
	free_bsm(bs_id);
	
	write2CR3(currpid);
	return OK;
}

