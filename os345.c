// os345.c - OS Kernel	09/12/2013
// ***********************************************************************
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// **                                                                   **
// ** The code given here is the basis for the BYU CS345 projects.      **
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
#include <time.h>
#include <assert.h>

#include "os345.h"
#include "os345signals.h"
#include "os345config.h"
#include "os345lc3.h"
#include "os345fat.h"

// **********************************************************************
//	local prototypes
//
void pollInterrupts(void);
static int scheduler(void);
static int dispatcher(void);

static void keyboard_isr(void);
static void timer_isr(void);

int sysKillTask(int taskId);
static int initOS(void);

int delta_clock_task(int, char**);

// **********************************************************************
// **********************************************************************
// global semaphores

Semaphore* semaphoreList;			// linked list of active semaphores

Semaphore* keyboard;				// keyboard semaphore
Semaphore* charReady;				// character has been entered
Semaphore* inBufferReady;			// input buffer ready semaphore

Semaphore* tics1sec;				// 1 second semaphore
Semaphore* tics10sec;
Semaphore* tics10thsec;				// 1/10 second semaphore

Semaphore* deltaClock;

// **********************************************************************
// **********************************************************************
// global system variables

TCB tcb[MAX_TASKS];					// task control block
Semaphore* taskSems[MAX_TASKS];		// task semaphore
jmp_buf k_context;					// context of kernel stack
jmp_buf reset_context;				// context of kernel stack
volatile void* temp;				// temp pointer used in dispatcher

int scheduler_mode;					// scheduler mode
int superMode;						// system mode
int curTask;						// current task #
long swapCount;						// number of re-schedule cycles
char inChar;						// last entered character
int charFlag;						// 0 => buffered input
int inBufIndx;						// input pointer into input buffer
char inBuffer[INBUF_SIZE+1];		// character input buffer
//Message messages[NUM_MESSAGES];		// process message buffers

int pollClock;						// current clock()
int lastPollClock;					// last pollClock
bool diskMounted;					// disk has been mounted

time_t oldTime1;					// old 1sec time
time_t oldTime10;
clock_t myClkTime;
clock_t myOldClkTime;


// **********************************************************************
// **********************************************************************
// OS startup
//
// 1. Init OS
// 2. Define reset longjmp vector
// 3. Define global system semaphores
// 4. Create CLI task
// 5. Enter scheduling/idle loop
//
int main(int argc, char* argv[])
{

	// Disable buffering for debugging
	// setbuf(stdout, NULL);
/*
	insertDeltaClock(4, tics10thsec);
	insertDeltaClock(8, tics10thsec);
	insertDeltaClock(1, tics10thsec);
	insertDeltaClock(12, tics10thsec);
	insertDeltaClock(6, tics10thsec);
	insertDeltaClock(1, tics10thsec);
	insertDeltaClock(2, tics10thsec);
	insertDeltaClock(9, tics10thsec);


	dc;
	int i = 0;
	for(i=0; i<deltaClockSize; i++) {
		printf("\n%i: %i seconds - %s", i, dc[i].time, dc[i].sem->name);
	}
*/

	// save context for restart (a system reset would return here...)
	int resetCode = setjmp(reset_context);
	superMode = TRUE;						// supervisor mode

	switch (resetCode)
	{
		case POWER_DOWN_QUIT:				// quit
			powerDown(0);
			printf("\nGoodbye!!");
			return 0;

		case POWER_DOWN_RESTART:			// restart
			powerDown(resetCode);
			printf("\nRestarting system...\n");

		case POWER_UP:						// startup
			break;

		default:
			printf("\nShutting down due to error %d", resetCode);
			powerDown(resetCode);
			return resetCode;
	}

	// output header message
	printf("%s", STARTUP_MSG);

	// initalize OS
	if ( resetCode = initOS()) return resetCode;

	// create global/system semaphores here
	//?? vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

	charReady = createSemaphore("charReady", BINARY, 0);
	inBufferReady = createSemaphore("inBufferReady", BINARY, 0);
	keyboard = createSemaphore("keyboard", BINARY, 1);
	tics1sec = createSemaphore("tics1sec", BINARY, 0);
	tics10thsec = createSemaphore("tics10thsec", BINARY, 0);
	tics10sec = createSemaphore("tics10sec", COUNTING, 0);
	deltaClock = createSemaphore("deltaClock", BINARY, 0);

	//?? ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

	// schedule CLI task
	createTask("myShell",			// task name
					P1_shellTask,	// task
					MED_PRIORITY,	// task priority
					argc,			// task arg count
					argv);			// task argument pointers

	/*
	static char* dcArgv[] = {"deltaClock"};
	createTask("deltaClock",
					delta_clock_task,
					HIGH_PRIORITY,
					1,
					dcArgv);
	*/
	// HERE WE GO................

	// Scheduling loop
	// 1. Check for asynchronous events (character inputs, timers, etc.)
	// 2. Choose a ready task to schedule
	// 3. Dispatch task
	// 4. Loop (forever!)

	while(1)									// scheduling loop
	{
		// check for character / timer interrupts
		pollInterrupts();

		// schedule highest priority ready task
		if ((curTask = scheduler()) < 0) continue;

		// dispatch curTask, quit OS if negative return
		if (dispatcher() < 0) break;
	}											// end of scheduling loop

	// exit os
	longjmp(reset_context, POWER_DOWN_QUIT);

	return 0;
} // end main



// **********************************************************************
// **********************************************************************
// scheduler
//
static int scheduler()
{
	int nextTask, i, j, childCount, percent, timeSlice;

	if( scheduler_mode == 0 ) {
		// Round Robin Scheduling
		nextTask = deque(rq, -1);

		if (nextTask >= 0)
		{
			enque(rq, nextTask, tcb[nextTask].priority);
		}
	}
	else {
		bool found = FALSE;
		// Fair Scheduling - look for non-zero task to schedule
		for(i=1; i <= rq[0]; i++) {
			if(tcb[rq[i]].time > 0) {
				nextTask = rq[i];
				tcb[rq[i]].time--;
				found = TRUE;
				break;
			}
		}

		if(! found) {
			// didn't find a task. All are 0. Loop through to find a possible parent
			for(i=1; i <= rq[0]; i++) {
				// Has this task already been alloted time this pass?
				if(tcb[rq[i]].time == 0) {
					// If not, lets check if it has any children
					childCount = 0;
					for(j=1; j <= rq[0]; j++) {
						if(tcb[rq[j]].parent == rq[i]) {
							childCount++;
						}
					}

					if((childCount == 0)) {
						//tcb[rq[i]].time = 1;
						continue;
					}

					// we've counted all the siblings
					percent = 100/NUM_PARENTS;
					timeSlice = percent / childCount;

					// Set sibling timeSlices
					for(j=1; j <= rq[0]; j++) {
						if(tcb[rq[j]].parent == rq[i]) {
							tcb[rq[j]].time = timeSlice;
						}
					}

					// set parent timeSlices
					tcb[rq[i]].time = percent % childCount;
				}
			}
			nextTask = rq[1]; // Give shell an occasional run
		}
	}

	if (tcb[nextTask].signal & mySIGSTOP) return -1;
	//printf("(%i)", nextTask);
	return nextTask;

} // end scheduler




// **********************************************************************
// **********************************************************************
// delta clock
//
int insertDeltaClock(int time, Semaphore* sem) {
	int insertPosition = 0;
	int timeRemaining = time;
	int ticsFromPrevious = 0;
	int ticsToNext = 0;
	int temp, i;

	// create new entry for delta clock with parameters
	struct DcEntry newEntry;
	newEntry.sem = sem;

	if(deltaClockSize == 0) {
		newEntry.time = time;
		dc[0] = newEntry;
	}

	else {
		for (insertPosition = 0; insertPosition < deltaClockSize; insertPosition++) {
			ticsFromPrevious = timeRemaining;
			timeRemaining = timeRemaining - dc[insertPosition].time;
			if (timeRemaining < 0) {
				ticsToNext = -1 * timeRemaining;
				break;
			}
			ticsFromPrevious = timeRemaining;
		}

		for (i = deltaClockSize-1; i >= insertPosition; i--) {
			dc[i+1] = dc[i];
		}

		// adjust times of new entry, and next entry
		newEntry.time = ticsFromPrevious;
		if(insertPosition != deltaClockSize)
			dc[insertPosition+1].time = ticsToNext;

		dc[insertPosition] = newEntry;
	}

	deltaClockSize ++;

	return 0;
}

int delta_clock_task(int argc, char* argv[]) {
	while(1) {
		printf("a");
	}
}

int tickDeltaClock() {
	int i;
	// Is it time to signal any semaphores?
	if(deltaClockSize && deltaClockTicker-- < 0 && --(dc[0].time) <= 0) {
		// For every entry that just expired, signal semaphore
		while(deltaClockSize > 0 && dc[0].time <= 0) {
			//printf("Signaling %s\n", dc[0].sem->name);
			SEM_SIGNAL(dc[0].sem);
			// shift everything down
			for(i=0; i<deltaClockSize-1; i++) {
				dc[i] = dc[i+1];
			}
			deltaClockSize--;
		}

		deltaClockTicker = 10;
	}
}


// **********************************************************************
// **********************************************************************
// enque
//
int enque(PriorityQueue queue, TID tid, Priority p) {
	int i;
	for (i=queue[0]; i>0; i--) {
		if (tcb[queue[i]].priority >= p) queue[i+1] = queue[i];
		else break;
	}

	queue[i+1] = tid;
	queue[0]++;

	return tid;
}


// **********************************************************************
// **********************************************************************
// deque
//
int deque(PriorityQueue queue, TID tid) {
	int i;
	bool found = FALSE;

	// return the highest priority task, if there's at least one
	if(tid == -1 && queue[0] > 0) {
		return queue[queue[0]--];
	}

	// the caller must have asked for a specific task
	for (i=1; i<=queue[0]; i++) {
		if (queue[i] == tid) found = TRUE;
		else if (found) queue[i-1] = queue[i];
	}

	if(found)
		queue[0]--;

	return (found) ? tid : -1;

}


// **********************************************************************
// **********************************************************************
// dispatch curTask
//
static int dispatcher()
{
	int result;

	// schedule task
	switch(tcb[curTask].state)
	{
		case S_NEW:
		{
			// new task
			//printf("\nNew Task[%d] %s", curTask, tcb[curTask].name);
			tcb[curTask].state = S_RUNNING;	// set task to run state

			// save kernel context for task SWAP's
			if (setjmp(k_context))
			{
				superMode = TRUE;					// supervisor mode
				break;								// context switch to next task
			}

			// move to new task stack (leave room for return value/address)
			temp = (int*)tcb[curTask].stack + (STACK_SIZE-8);
			SET_STACK(temp);
			superMode = FALSE;						// user mode

			// begin execution of new task, pass argc, argv
			result = (*tcb[curTask].task)(tcb[curTask].argc, tcb[curTask].argv);

			// task has completed
			if (result) printf("\nTask[%d] returned %d", curTask, result);
			else printf("\nTask[%d] returned %d", curTask, result);
			tcb[curTask].state = S_EXIT;			// set task to exit state

			// return to kernal mode
			longjmp(k_context, 1);					// return to kernel
		}

		case S_READY:
		{
			tcb[curTask].state = S_RUNNING;			// set task to run
		}

		case S_RUNNING:
		{
			if (setjmp(k_context))
			{
				// SWAP executed in task
				superMode = TRUE;					// supervisor mode
				break;								// return from task
			}
			if (signals()) break;
			longjmp(tcb[curTask].context, 3); 		// restore task context
		}

		case S_BLOCKED:
		{
			break;
		}

		case S_EXIT:
		{
			if (curTask == 0) return -1;			// if CLI, then quit scheduler
			// release resources and kill task
			sysKillTask(curTask);					// kill current task
			break;
		}

		default:
		{
			printf("Unknown Task[%d] State", curTask);
			longjmp(reset_context, POWER_DOWN_ERROR);
		}
	}
	return 0;
} // end dispatcher



// **********************************************************************
// **********************************************************************
// Do a context switch to next task.

// 1. If scheduling task, return (setjmp returns non-zero value)
// 2. Else, save current task context (setjmp returns zero value)
// 3. Set current task state to READY
// 4. Enter kernel mode (longjmp to k_context)

void swapTask()
{
	assert("SWAP Error" && !superMode);		// assert user mode

	// increment swap cycle counter
	swapCount++;

	// either save current task context or schedule task (return)
	if (setjmp(tcb[curTask].context))
	{
		superMode = FALSE;					// user mode
		return;
	}

	// context switch - move task state to ready
	if (tcb[curTask].state == S_RUNNING) tcb[curTask].state = S_READY;

	// move to kernel mode (reschedule)
	longjmp(k_context, 2);
} // end swapTask



// **********************************************************************
// **********************************************************************
// system utility functions
// **********************************************************************
// **********************************************************************

// **********************************************************************
// **********************************************************************
// initialize operating system
static int initOS()
{
	int i;

	// make any system adjustments (for unblocking keyboard inputs)
	INIT_OS

	// reset system variables
	curTask = 0;						// current task #
	swapCount = 0;						// number of scheduler cycles
	scheduler_mode = 0;					// default scheduler
	inChar = 0;							// last entered character
	charFlag = 0;						// 0 => buffered input
	inBufIndx = 0;						// input pointer into input buffer
	semaphoreList = 0;					// linked list of active semaphores
	diskMounted = 0;					// disk has been mounted

	// malloc ready queue
	rq = (int*)malloc(MAX_TASKS * sizeof(int));
	if (rq == NULL) return 99;

	// capture current time
	lastPollClock = clock();			// last pollClock
	time(&oldTime1);
	time(&oldTime10);

	// init system tcb's
	for (i=0; i<MAX_TASKS; i++)
	{
		tcb[i].name = NULL;				// tcb
		taskSems[i] = NULL;				// task semaphore
	}

	// init tcb
	for (i=0; i<MAX_TASKS; i++)
	{
		tcb[i].name = NULL;
	}

	// initialize lc-3 memory
	initLC3Memory(LC3_MEM_FRAME, 0xF800>>6);

	// ?? initialize all execution queues
	rq[0] = 0;

	dc = (DcEntry*)malloc(MAX_DC_SIZE * sizeof(DcEntry));
	deltaClockSize = 0;
	deltaClockTicker = 0;

	bq = (int*)malloc(MAX_TASKS * sizeof(int));
	bq[0] = 0;

	return 0;
} // end initOS



// **********************************************************************
// **********************************************************************
// Causes the system to shut down. Use this for critical errors
void powerDown(int code)
{
	int i;
	printf("\nPowerDown Code %d", code);

	// release all system resources.
	printf("\nRecovering Task Resources...");

	// kill all tasks
	for (i = MAX_TASKS-1; i >= 0; i--)
		if(tcb[i].name) sysKillTask(i);

	// delete all semaphores
	while (semaphoreList)
		deleteSemaphore(&semaphoreList);

	// free ready queue
	free(rq);

	// ?? release any other system resources
	// ?? deltaclock (project 3)

	RESTORE_OS
	return;
} // end powerDown
