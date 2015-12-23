/* initialize.c - nulluser, sizmem, sysinit */

#include <conf.h>
#include <i386.h>
#include <kernel.h>
#include <proc.h>
#include <sem.h>
#include <sleep.h>
#include <mem.h>
#include <tty.h>
#include <q.h>
#include <io.h>
#include <paging.h>

/*#define DETAIL */
#define HOLESIZE	(600)	
#define	HOLESTART	(640 * 1024)
#define	HOLEEND		((1024 + HOLESIZE) * 1024)  
/* Extra 600 for bootp loading, and monitor */

extern int main(); /* address of user's main prog	*/
extern int start();
LOCAL sysinit();

fifo_q fifo_hd;
int Fifo_npages;
int LRU_Count;

/* Declarations of major kernel variables */
struct pentry proctab[NPROC]; /* process table			*/
int nextproc; /* next process slot to use in create	*/
struct sentry semaph[NSEM]; /* semaphore table			*/
int nextsem; /* next sempahore slot to use in screate*/
struct qent q[NQENT]; /* q table (see queue.c)		*/
int nextqueue; /* next slot in q structure to use	*/
char *maxaddr; /* max memory address (set by sizmem)	*/
struct mblock memlist; /* list of free memory blocks		*/
#ifdef	Ntty
struct tty tty[Ntty]; /* SLU buffers and mode control		*/
#endif

/* active system status */
int numproc; /* number of live user processes	*/
int currpid; /* id of currently running process	*/
int reboot = 0; /* non-zero after first boot		*/

int rdyhead, rdytail; /* head/tail of ready list (q indicies)	*/
char vers[80];
int console_dev; /* the console device			*/

/*  added for the demand paging */
int page_replace_policy = FIFO;

/************************************************************************/
/***				NOTE:				      ***/
/***								      ***/
/***   This is where the system begins after the C environment has    ***/
/***   been established.  Interrupts are initially DISABLED, and      ***/
/***   must eventually be enabled explicitly.  This routine turns     ***/
/***   itself into the null process after initialization.  Because    ***/
/***   the null process must always remain ready to run, it cannot    ***/
/***   execute code that might cause it to be suspended, wait for a   ***/
/***   semaphore, or put to sleep, or exit.  In particular, it must   ***/
/***   not do I/O unless it uses kprintf for polled output.           ***/
/***								      ***/
/************************************************************************/

/*------------------------------------------------------------------------
 *  nulluser  -- initialize system and become the null process (id==0)
 *------------------------------------------------------------------------
 */
nulluser() /* babysit CPU when no one is home */
{
	int userpid;

	console_dev = SERIAL0; /* set console to COM0 */

	initevec();

	kprintf("\n**NEW! NEW! system running up!\n");
	sysinit();

	enable(); /* enable interrupts */

	sprintf(vers, "PC Xinu %s", VERSION);
	kprintf("\n\n%s\n", vers);
	if (reboot++ < 1)
		kprintf("\n");
	else
		kprintf("   (reboot %d)\n", reboot);

	kprintf("%d bytes real mem\n", (unsigned long) maxaddr + 1);
#ifdef DETAIL	
	kprintf("    %d", (unsigned long) 0);
	kprintf(" to %d\n", (unsigned long) (maxaddr) );
#endif	

	kprintf("%d bytes Xinu code\n", (unsigned long) ((unsigned long) &end - (unsigned long) start));
#ifdef DETAIL	
	kprintf("    %d", (unsigned long) start);
	kprintf(" to %d\n", (unsigned long) &end );
#endif

#ifdef DETAIL	
	kprintf("%d bytes user stack/heap space\n",
			(unsigned long) ((unsigned long) maxaddr - (unsigned long) &end));
	kprintf("    %d", (unsigned long) &end);
	kprintf(" to %d\n", (unsigned long) maxaddr);
#endif	

	kprintf("clock %sabled\n", clkruns == 1 ? "en" : "dis");

	/* create a process to execute the user's main program */
	kprintf("Calling create to start main\n");
	userpid = create(main, INITSTK, INITPRIO, INITNAME, INITARGS);
	resume(userpid);

	while (TRUE)
		/* empty */;
}

/*------------------------------------------------------------------------
 *  sysinit  --  initialize all Xinu data structeres and devices
 *------------------------------------------------------------------------
 */
LOCAL sysinit() {
	static long currsp;
	int i, j;
	struct pentry *pptr;
	struct sentry *sptr;
	struct mblock *mptr;
	SYSCALL pfintr();

	numproc = 0; /* initialize system variables */
	nextproc = NPROC - 1;
	nextsem = NSEM - 1;
	nextqueue = NPROC; /* q[0..NPROC-1] are processes */

	/* initialize free memory list */
	/* PC version has to pre-allocate 640K-1024K "hole" */
	if (maxaddr + 1 > HOLESTART) {
		memlist.mnext = mptr = (struct mblock *) roundmb(&end);
		mptr->mnext = (struct mblock *) HOLEEND;
		mptr->mlen = (int) truncew(((unsigned) HOLESTART - (unsigned) &end));
		mptr->mlen -= 4;

		mptr = (struct mblock *) HOLEEND;
		mptr->mnext = 0;
		mptr->mlen = (int) truncew((unsigned) maxaddr - HOLEEND -
		NULLSTK);
		/*
		 mptr->mlen = (int) truncew((unsigned)maxaddr - (4096 - 1024 ) *  4096 - HOLEEND - NULLSTK);
		 */
	} else {
		/* initialize free memory list */
		memlist.mnext = mptr = (struct mblock *) roundmb(&end);
		mptr->mnext = 0;
		mptr->mlen = (int) truncew((unsigned) maxaddr - (int) &end -
		NULLSTK);
	}

	for (i = 0; i < NPROC; i++) /* initialize process table */
		proctab[i].pstate = PRFREE;

#ifdef	MEMMARK
	_mkinit(); /* initialize memory marking */
#endif

#ifdef	RTCLOCK
	clkinit(); /* initialize r.t.clock	*/
#endif

	mon_init(); /* init monitor */

#ifdef NDEVS
	for (i=0; i<NDEVS; i++ ) {
		init_dev(i);
	}
#endif

	/*
	 *
	 */
//	kprintf("\nd: Initializing Demand Paging Data Structures\n");
	//Initialize all necessary data structures
	LRU_Count = 0;
	init_bsm();
	init_frm();

	//Install the page fault interrup service routine
	set_evec(14, pfintr);
	//Create the page tables which will map pages 0 through 4095 to the physical 16 MB. These will be called the global page tables.
//	initializeGlobalPageTable();
	int gb_frm;
	for (i = 0; i < 4; i++) {
		gb_frm= create_PageTable(NULLPROC);
		if (gb_frm== -1) {
			return SYSERR;
		}
		GPT[i] = FRAME0 +gb_frm;
		pt_t *pt = GPT[i] * NBPG;
		for (j = 0; j < 1024; j++) {
			//pt_t *pt = GPT[i] * NBPG + j * sizeof(pt_t);
                        frm_tab[gb_frm].fr_refcnt++;

			pt[j].pt_pres = 1;
			pt[j].pt_write = 1;
			pt[j].pt_base = j + i * 1024;

//			frm_tab[gb_frm].fr_refcnt++;
		}
	}



	//Allocate and initialize a page directory for NULL Process
	create_PageDirectory(NULLPROC);
	//set PDBR CR3 Register to the page directory of NULL Process.
	write2CR3(NULLPROC);
//	kprintf("wrote to cr3 while initializing\n");
	//Enable Paging
//	kprintf("Enable paging\n");
	enable_paging();

	/*
	 *
	 */
	pptr = &proctab[NULLPROC]; /* initialize null process entry */
	pptr->pstate = PRCURR;
	for (j = 0; j < 7; j++)
		pptr->pname[j] = "prnull"[j];
	pptr->plimit = (WORD) (maxaddr + 1) - NULLSTK;
	pptr->pbase = (WORD) maxaddr - 3;
	/*
	 pptr->plimit = (WORD)(maxaddr + 1) - NULLSTK - (4096 - 1024 )*4096;
	 pptr->pbase = (WORD) maxaddr - 3 - (4096-1024)*4096;
	 */
	pptr->pesp = pptr->pbase - 4; /* for stkchk; rewritten before used */
	*((int *) pptr->pbase) = MAGIC;
	pptr->paddr = (WORD) nulluser;
	pptr->pargs = 0;
	pptr->pprio = 0;
	currpid = NULLPROC;

	//Initializing with dummy demand paging parameters in NULL Process Entry
	fifo_hd.frameID = -1;
	fifo_hd.next = NULL;
	Fifo_npages = 0;

//	kprintf("\nd: Initializing Demand Paging Process Entry Parameters\n");
	pptr->store = -1;
	pptr->vhpno = -1;
	pptr->vhpnpages = -1;
	pptr->vmemlist = NULL;
	for (i = 0; i < NUM_BACKING_STORES; i++) {
		pptr->BSMapping[i].bs_PrivHeap = 0;
		pptr->BSMapping[i].bs_npages = 0;
		pptr->BSMapping[i].bs_pid = -1;
		pptr->BSMapping[i].bs_refCount = 0;
		pptr->BSMapping[i].bs_sem = -1;
		pptr->BSMapping[i].bs_status = UNMAPPED;
		pptr->BSMapping[i].bs_vpno = -1;
	}
	//

	for (i = 0; i < NSEM; i++) { /* initialize semaphores */
		(sptr = &semaph[i])->sstate = SFREE;
		sptr->sqtail = 1 + (sptr->sqhead = newqueue());
	}

	rdytail = 1 + (rdyhead = newqueue());/* initialize ready list */
	return (OK);
}

stop(s)
	char *s; {
	kprintf("%s\n", s);
	kprintf("looping... press reset\n");
	while (1)
		/* empty */;
}

delay(n)
	int n; {
	DELAY(n);
}

#define	NBPG	4096

/*------------------------------------------------------------------------
 * sizmem - return memory size (in pages)
 *------------------------------------------------------------------------
 */
long sizmem() {
	unsigned char *ptr, *start, stmp, tmp;
	int npages;

	/* at least now its hacked to return
	 the right value for the Xinu lab backends (16 MB) */

	return 4096;

	start = ptr = 0;
	npages = 0;
	stmp = *start;
	while (1) {
		tmp = *ptr;
		*ptr = 0xA5;
		if (*ptr != 0xA5)
			break;
		*ptr = tmp;
		++npages;
		ptr += NBPG;
		if ((int) ptr == HOLESTART) { /* skip I/O pages */
			npages += (1024 - 640) / 4;
			ptr = (unsigned char *) HOLEEND;
		}
	}
	return npages;
}
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
