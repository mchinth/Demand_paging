#include <conf.h>
#include <kernel.h>
#include <mark.h>
#include <bufpool.h>
#include <proc.h>
#include <paging.h>

SYSCALL read_bs(char *dst, bsd_t store, int page) {
	STATWORD ps;
	disable(ps);
	if (store <0 || store >15 || page <0|| page>127) {
		//kprintf("\n SYSERR in reading ");
		restore(ps);
		return SYSERR;
	}

	void * phy_addr = BACKING_STORE_BASE + (store << 19) + (page * NBPG);
	//kprintf("\n phy_addr %x  \n ", phy_addr);
	bcopy(phy_addr, (void*) dst, NBPG);
	restore(ps);
	return OK;
}

