// os345mmu.c - LC-3 Memory Management Unit	03/12/2015
//
//		03/12/2015	added PAGE_GET_SIZE to accessPage()
//
// **************************************************************************
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// **                                                                   **
// ** The code given here is the basis for the CS345 projects.          **
// ** It comes "as is" and "unwarranted."  As such, when you use part   **
// ** or all of the code, it becomes "yours" and you are responsible to **
// ** understand any algorithm or method presented.  Likewise, any      **
// ** errors or problems become your responsibility to fix.             **
// **                                                                   **
// ** NOTES:                                                            **
// ** -Comments beginning with "// ??" may require some implementation. **
// ** -Tab stops are set at every 3 spaces.                             **
// ** -The function API's in "OS345.h" should not be altered.           **
// **                                                                   **
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// ***********************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <assert.h>
#include "os345.h"
#include "os345lc3.h"

// ***********************************************************************
// mmu variables

// LC-3 memory
unsigned short int memory[LC3_MAX_MEMORY];

// statistics
int memAccess;						// memory accesses
int memHits;						// memory hits
int memPageFaults;					// memory faults
int swapcount;

int getFrame(int);
int getAvailableFrame(void);

extern int rptcl;				// Which RPT are we examining
extern int uptcl;							// Which UPT are we examining
int uptcl_end;
int clearableRPT = 0;

extern TCB tcb[];					// task control block
extern int curTask;					// current task #

int getFrame(int notme)
{
	int frame, rootword1, rootword2, userword1, userword2, frameEnd;
	//int frame, found;
	frame = getAvailableFrame();
	if (frame >=0) return frame;

	while(frame < 0) {
		while(rptcl < LC3_RPT_END && frame < 0) {
			rootword1 = memory[rptcl];					// FDRP__ffffffffff
			rootword2 = memory[rptcl+1];

			if(DEFINED(rootword1)) {
				// This is a root page table
				if(REFERENCED(rootword1)) {
					if(clearableRPT) {
						rootword1 = memory[rptcl] = CLEAR_REF(rootword1);
						clearableRPT = 0;
					}
				}
				else {
					uptcl_end = (FRAME(rootword1)<<6) + 64;
					if(uptcl < (FRAME(rootword1)<<6) || uptcl >= uptcl_end)
						uptcl = (FRAME(rootword1)<<6);

					while(uptcl < uptcl_end && frame < 0) {
						userword1 = memory[uptcl];
						userword2 = memory[uptcl+1];

						if(DEFINED(userword1)) {
							if(REFERENCED(userword1)) {
								userword1 = memory[uptcl] = CLEAR_REF(userword1);
							}
							else {
								if(FRAME(userword1) != notme) {
									frame = FRAME(userword1);
									userword1 = memory[uptcl] = 0;
									int pagenum = accessPage(0, frame, PAGE_NEW_WRITE);
									userword2 = memory[uptcl+1] = SET_PAGED(pagenum);
								}
							}
						}
						uptcl += 2;
					}
				}
			}
			if(frame < 0) {
				rptcl += 2;
				clearableRPT = 1;
			}
		}
		if(frame < 0)
			rptcl = LC3_RPT;
	}
	return frame;
}
// **************************************************************************
// **************************************************************************
// LC3 Memory Management Unit
// Virtual Memory Process
// **************************************************************************
//           ___________________________________Frame defined
//          / __________________________________Dirty frame
//         / / _________________________________Referenced frame
//        / / / ________________________________Pinned in memory
//       / / / /     ___________________________
//      / / / /     /                 __________frame # (0-1023) (2^10)
//     / / / /     /                 / _________page defined
//    / / / /     /                 / /       __page # (0-4096) (2^12)
//   / / / /     /                 / /       /
//  / / / /     / 	              / /       /
// F D R P - - f f|f f f f f f f f|S - - - p p p p|p p p p p p p p

#define MMU_ENABLE	1

unsigned short int *getMemAdr(int va, int rwFlg)
{
	unsigned short int rpta, rpte1, rpte2;
	unsigned short int upta, upte1, upte2;
	int i, uptFrame, dataFrame;

	if (va < 0x3000) return &memory[va];

	rpta = TASK_RPT + RPTI(va);
	rpte1 = MEMWORD(rpta);
	rpte2 = MEMWORD(rpta+1);

	memAccess++;

	if(DEFINED(rpte1)) {
		memHits ++;
		uptFrame = FRAME(rpte1);
	}

	else
	{
		memPageFaults ++;
		uptFrame = getFrame(-1);
		rpte1 = SET_DEFINED(uptFrame);
		if (PAGED(rpte2)) {
			accessPage(SWAPPAGE(rpte2), uptFrame, PAGE_READ);
		}
		else {
			rpte1 = SET_DIRTY(rpte1);
			rpte2 = 0;
			for(i=0; i<64; i++)
				MEMWORD((uptFrame<<6) + i) = 0;
		}
	}

	MEMWORD(rpta) = SET_REF(SET_PINNED(rpte1));
	MEMWORD(rpta+1) = rpte2;

	upta = (FRAME(rpte1) << 6) + UPTI(va);
	upte1 = MEMWORD(upta);
	upte2 = MEMWORD(upta+1);

	memAccess++;

	if(DEFINED(upte1))
		memHits++;
	else {
		memPageFaults ++;
		dataFrame = getFrame(uptFrame);
		upte1 = SET_DEFINED(dataFrame);
		if (PAGED(upte2)) {
			accessPage(SWAPPAGE(upte2), dataFrame, PAGE_READ);
		}
		else {
			upte1 = SET_DIRTY(upte1);
			upte2 = 0;
			for(i=0; i<64; i++)
				MEMWORD((dataFrame<<6) + i) = 0xf025;
		}
	}

	if(rwFlg) {
		upte1 = SET_DIRTY(upte1);
		rpte1 = SET_DIRTY(rpte1);
	}

	MEMWORD(upta) = upte1 = SET_REF(upte1);
	MEMWORD(upta+1) = upte2;

	return &memory[(FRAME(upte1)<<6) + FRAMEOFFSET(va)];

} // end getMemAdr


// **************************************************************************
// **************************************************************************
// set frames available from sf to ef
//    flg = 0 -> clear all others
//        = 1 -> just add bits
//
void setFrameTableBits(int flg, int sf, int ef)
{	int i, data;
	int adr = LC3_FBT-1;             // index to frame bit table
	int fmask = 0x0001;              // bit mask

	// 1024 frames in LC-3 memory
	for (i=0; i<LC3_FRAMES; i++)
	{	if (fmask & 0x0001)
		{  fmask = 0x8000;
			adr++;
			data = (flg)?MEMWORD(adr):0;
		}
		else fmask = fmask >> 1;
		// allocate frame if in range
		if ( (i >= sf) && (i < ef)) data = data | fmask;
		MEMWORD(adr) = data;
	}
	return;
} // end setFrameTableBits


// **************************************************************************
// get frame from frame bit table (else return -1)
int getAvailableFrame()
{
	int i, data;
	int adr = LC3_FBT - 1;				// index to frame bit table
	int fmask = 0x0001;					// bit mask

	for (i=0; i<LC3_FRAMES; i++)		// look thru all frames
	{	if (fmask & 0x0001)
		{  fmask = 0x8000;				// move to next work
			adr++;
			data = MEMWORD(adr);
		}
		else fmask = fmask >> 1;		// next frame
		// deallocate frame and return frame #
		if (data & fmask)
		{  MEMWORD(adr) = data & ~fmask;
			return i;
		}
	}
	return -1;
} // end getAvailableFrame



// **************************************************************************
// read/write to swap space
int accessPage(int pnum, int frame, int rwnFlg)
{
	static int nextPage;						// swap page size
	static int pageReads;						// page reads
	static int pageWrites;						// page writes
	static unsigned short int swapMemory[LC3_MAX_SWAP_MEMORY];

	if ((nextPage >= LC3_MAX_PAGE) || (pnum >= LC3_MAX_PAGE))
	{
		printf("\nVirtual Memory Space Exceeded!  (%d)", LC3_MAX_PAGE);
		exit(-4);
	}
	switch(rwnFlg)
	{
		case PAGE_INIT:                    		// init paging
			memAccess = 0;						// memory accesses
			memHits = 0;						// memory hits
			memPageFaults = 0;					// memory faults
			nextPage = 0;						// disk swap space size
			pageReads = 0;						// disk page reads
			pageWrites = 0;						// disk page writes
			return 0;

		case PAGE_GET_SIZE:                    	// return swap size
			return nextPage;

		case PAGE_GET_READS:                   	// return swap reads
			return pageReads;

		case PAGE_GET_WRITES:                    // return swap writes
			return pageWrites;

		case PAGE_GET_ADR:                    	// return page address
			return (int)(&swapMemory[pnum<<6]);

		case PAGE_NEW_WRITE:                   // new write (Drops thru to write old)
			pnum = nextPage++;

		case PAGE_OLD_WRITE:                   // write
			//printf("\n    (%d) Write frame %d (memory[%04x]) to page %d", p.PID, frame, frame<<6, pnum);
			memcpy(&swapMemory[pnum<<6], &memory[frame<<6], 1<<7);
			pageWrites++;
			return pnum;

		case PAGE_READ:                    	// read
			//printf("\n    (%d) Read page %d into frame %d (memory[%04x])", p.PID, pnum, frame, frame<<6);
			memcpy(&memory[frame<<6], &swapMemory[pnum<<6], 1<<7);
			pageReads++;
			return pnum;

		case PAGE_FREE:                   // free page
			printf("\nPAGE_FREE not implemented");
			break;
   }
   return pnum;
} // end accessPage
