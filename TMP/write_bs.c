#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <mark.h>
#include <bufpool.h>
#include <paging.h>

int write_bs(char *src, bsd_t store, int page) {
	STATWORD ps;
disable(ps);
	if (store <0 ||store >15 || page <0||page> 127) {
	restore(ps);
	return SYSERR;
	}

	char * phy_addr = BACKING_STORE_BASE + (store << 19) + (page * NBPG);
	//kprintf("\n phy addr write_bs %x ", phy_addr);
	bcopy((void*) src, phy_addr, NBPG);
restore(ps);
	return OK;
}

