// os345p3.c - Jurassic Park
// ***********************************************************************
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
#include <time.h>
#include <assert.h>
#include "os345.h"
#include "os345park.h"

// ***********************************************************************
// project 3 variables

// Jurassic Park
extern JPARK myPark;
extern Semaphore* parkMutex;						// protect park access
extern Semaphore* fillSeat[NUM_CARS];			// (signal) seat ready to fill
extern Semaphore* seatFilled[NUM_CARS];		// (wait) passenger seated
extern Semaphore* rideOver[NUM_CARS];			// (signal) ride over
Semaphore* getPassenger;
Semaphore* driverLoaded;
Semaphore* seatTaken;
Semaphore* passengerSeated;
Semaphore* needDriver;
Semaphore* sellingTicket;
Semaphore* wakeupDriver;
Semaphore* needTicket;
Semaphore* takeTicket;
Semaphore* enterPark;
Semaphore* tickets;
Semaphore* giftShop;
Semaphore* museum;


// ***********************************************************************
// project 3 functions and tasks
void CL3_project3(int, char**);
void CL3_dc(int, char**);

int jurassic_car(int argc, char* argv[]);
int jurassic_visitor(int argc, char* argv[]);
int jurassic_driver(int argc, char* argv[]);

int loadingCar;
Semaphore* loadingDriver;
Semaphore* loadingPassenger;


// ***********************************************************************
// ***********************************************************************
// project3 command
int P3_project3(int argc, char* argv[])
	{
	char buf[32];
	char carName[5];
	char* newArgv[2];
	static char* carArgv[2];
	int i;
	srand(time(NULL));

	// start park
	sprintf(buf, "jurassicPark");
	newArgv[0] = buf;
	createTask( buf,				// task name
		jurassicTask,				// task
		MED_PRIORITY,				// task priority
		1,								// task count
		newArgv);					// task argument

	// wait for park to get initialized...
	while (!parkMutex) SWAP;
	printf("\nStart Jurassic Park...");		SWAP;
	//myPark.numInCarLine = myPark.numInPark = 4;


	getPassenger = createSemaphore("getPassenger", BINARY, 0);		SWAP;
	seatTaken = createSemaphore("seatTaken", BINARY, 0);		SWAP;
	passengerSeated = createSemaphore("passengerSeated", BINARY, 0);		SWAP;
	needDriver = createSemaphore("needDriver", BINARY, 0);		SWAP;
	wakeupDriver = createSemaphore("wakeupDriver", COUNTING, 0);		SWAP;
	driverLoaded = createSemaphore("driverLoaded", BINARY, 0);		SWAP;

	sellingTicket = createSemaphore("sellingTicket", BINARY, 1);		SWAP;
	needTicket = createSemaphore("needTicket", COUNTING, 0);		SWAP;
 	takeTicket = createSemaphore("takeTicket", BINARY, 0);		SWAP;
	enterPark = createSemaphore("enterPark", COUNTING, 0);		SWAP;
	tickets = createSemaphore("tickets", COUNTING, MAX_TICKETS);		SWAP;

	museum = createSemaphore("museum", COUNTING, MAX_IN_MUSEUM);		SWAP;
	giftShop = createSemaphore("giftShop", COUNTING, MAX_IN_GIFTSHOP);		SWAP;

	loadingCar = 0;		SWAP;

	static char* car1Argv[] = {"carA","0"};		SWAP;
	createTask("CarA", jurassic_car, MED_PRIORITY, 2, car1Argv);		SWAP;

	static char* car2Argv[] = {"carB","1"};		SWAP;
	createTask("CarB", jurassic_car, MED_PRIORITY, 2, car2Argv);		SWAP;

	static char* car3Argv[] = {"carC","2"};		SWAP;
	createTask("CarC", jurassic_car, MED_PRIORITY, 2, car3Argv);		SWAP;

	static char* car4Argv[] = {"carD","3"};		SWAP;
	createTask("CarD", jurassic_car, MED_PRIORITY, 2, car4Argv);		SWAP;

	for(i = 0; i < 45; i++) {
		char* newArgv[2];		SWAP;
		sprintf(buf, "visitor%i", i);		SWAP;
		newArgv[0] = buf;		SWAP;
		char id[3];		SWAP;
		sprintf(id, "%i", i);		SWAP;
		newArgv[1] = id;		SWAP;
		createTask(buf, jurassic_visitor, MED_PRIORITY, 2, newArgv);		SWAP;
	}

	static char* dr0Argv[] = {"driver0", "0"};		SWAP;
	createTask("driver0", jurassic_driver, MED_PRIORITY, 2, dr0Argv);		SWAP;
	static char* dr1Argv[] = {"driver1", "1"};		SWAP;
	createTask("driver1", jurassic_driver, MED_PRIORITY, 2, dr1Argv);		SWAP;
	static char* dr2Argv[] = {"driver2", "2"};		SWAP;
	createTask("driver2", jurassic_driver, MED_PRIORITY, 2, dr2Argv);		SWAP;
	static char* dr3Argv[] = {"driver3", "3"};		SWAP;
	createTask("driver3", jurassic_driver, MED_PRIORITY, 2, dr3Argv);		SWAP;


	return 0;
} // end project3

int jurassic_visitor(int argc, char* argv[]) {
	int passengerID = INTEGER(argv[1]);

	int delayTime;
	bool ridden = FALSE;
	Semaphore* delay;

	delay = createSemaphore(argv[0], BINARY, 0);	SWAP; 	// create notification event

	delayTime = rand() % 10;			SWAP
	insertDeltaClock( delayTime, delay);	SWAP
	SEM_WAIT(delay);							SWAP

	SEM_WAIT(parkMutex);					SWAP
	myPark.numOutsidePark ++;			SWAP
	SEM_SIGNAL(parkMutex);				SWAP

	delayTime = rand() % 3;			SWAP
	insertDeltaClock( delayTime, delay);	SWAP
	SEM_WAIT(delay);							SWAP

	SEM_WAIT(parkMutex);					SWAP
	myPark.numOutsidePark--;			SWAP
	myPark.numInPark++;						SWAP
	myPark.numInTicketLine++;			SWAP
	SEM_SIGNAL(parkMutex);				SWAP

	delayTime = rand() % 3;		SWAP;
	insertDeltaClock( delayTime, delay);		SWAP;
	SEM_WAIT(delay);		SWAP;

	SEM_SIGNAL(needTicket);				SWAP
	SEM_SIGNAL(wakeupDriver);			SWAP
	SEM_WAIT(takeTicket);					SWAP

	SEM_WAIT(parkMutex);					SWAP
	myPark.numInTicketLine--;			SWAP
	myPark.numTicketsAvailable--;
	myPark.numInMuseumLine++;			SWAP
	SEM_SIGNAL(parkMutex);				SWAP

	delayTime = rand() % 3;		SWAP;
	insertDeltaClock( delayTime, delay);		SWAP;
	SEM_WAIT(delay);		SWAP;

	SEM_WAIT(museum);		SWAP;
	SEM_WAIT(parkMutex);					SWAP
	myPark.numInMuseumLine--;			SWAP
	myPark.numInMuseum++;					SWAP
	SEM_SIGNAL(parkMutex);				SWAP
	SEM_SIGNAL(museum);		SWAP;

	delayTime = rand() % 3;		SWAP;
	insertDeltaClock( delayTime, delay);		SWAP;
	SEM_WAIT(delay);		SWAP;

	SEM_WAIT(parkMutex);		SWAP;
	myPark.numInMuseum--;		SWAP;
	myPark.numInCarLine++;		SWAP;
	SEM_SIGNAL(parkMutex);		SWAP;

	delayTime = rand() % 3;		SWAP;
	insertDeltaClock(delayTime, delay);		SWAP;
	SEM_WAIT(delay);		SWAP;

	SEM_WAIT(getPassenger);		SWAP;
	loadingPassenger = delay;		SWAP;
	SEM_SIGNAL(seatTaken);		SWAP;


	SEM_WAIT(parkMutex);		SWAP;
	myPark.numTicketsAvailable++;		SWAP;
	SEM_SIGNAL(parkMutex);		SWAP;
	SEM_SIGNAL(tickets); 		SWAP;// ??????		SWAP;

	SEM_WAIT(delay);		SWAP;

	SEM_WAIT(parkMutex);		SWAP;
	myPark.numInCars--;		SWAP;
	myPark.numInGiftLine++;		SWAP;
	SEM_SIGNAL(parkMutex);		SWAP;

	delayTime = rand() % 3;		SWAP;
	insertDeltaClock(delayTime, delay);		SWAP;
	SEM_WAIT(delay);		SWAP;

	SEM_WAIT(giftShop);		SWAP;
	SEM_WAIT(parkMutex);		SWAP;
	myPark.numInGiftShop++;		SWAP;
	myPark.numInGiftLine--;		SWAP;
	SEM_SIGNAL(parkMutex);		SWAP;
	SEM_SIGNAL(giftShop);		SWAP;

	delayTime = rand() % 3;		SWAP;
	insertDeltaClock(delayTime, delay);		SWAP;
	SEM_WAIT(delay);		SWAP;

	SEM_WAIT(parkMutex);		SWAP;
	myPark.numExitedPark++;		SWAP;
	myPark.numInGiftShop--;		SWAP;
	myPark.numInPark--;		SWAP;
	SEM_SIGNAL(parkMutex);		SWAP;

	return 0;
}

int jurassic_driver(int argc, char* argv[]) {
	int driverID = INTEGER(argv[1]);
	char buf[32];
	Semaphore* driverDone;
	sprintf(buf, "driverDone%d", driverID); 		SWAP;
	driverDone = createSemaphore(buf, BINARY, 0);	SWAP;

	while(1) {
		SEM_WAIT(wakeupDriver);		SWAP;

		if( iSEM_TRYLOCK(needDriver) ) {

			loadingDriver = driverDone;		SWAP;
			SEM_SIGNAL(driverLoaded);		SWAP;

			SEM_WAIT(parkMutex);		SWAP;
			myPark.drivers[driverID] = loadingCar + 1;		SWAP;
			SEM_SIGNAL(parkMutex);		SWAP;

			SEM_WAIT(driverDone);		SWAP;

			SEM_WAIT(parkMutex);		SWAP;
			myPark.drivers[driverID] = 0;		SWAP;
			SEM_SIGNAL(parkMutex);		SWAP;
		}
		else if( iSEM_TRYLOCK(needTicket) ) {
			//printf("Tickets Needed!");

			// Is someone else selling a ticket?
			SEM_WAIT(sellingTicket);		SWAP;

			SEM_WAIT(parkMutex);		SWAP;
			myPark.drivers[driverID] = -1;		SWAP;
			SEM_SIGNAL(parkMutex);		SWAP;

			// Wait for an available ticket
			SEM_WAIT(tickets);		SWAP;

			// Signal the visitor to take the ticket
			SEM_SIGNAL(takeTicket);		SWAP;

			SEM_WAIT(parkMutex);		SWAP;
			myPark.drivers[driverID] = 0;		SWAP;
			SEM_SIGNAL(parkMutex);		SWAP;

			SEM_SIGNAL(sellingTicket);		SWAP;
		}
		else {
			printf("What!?!?\n");		SWAP;
		}
	}

	/*
	char buf[32];
	Semaphore* driverDone;
	int myID = atoi(argv[1]) - 1;		SWAP;	// get unique drive id
	printf(buf, "Starting driverTask%d", myID);		SWAP;
	sprintf(buf, "driverDone%d", myID + 1); 		SWAP;
	driverDone = createSemaphore(buf, BINARY, 0);	SWAP; 	// create notification event

	while(1)				// such is my life!!
	{
		mySEM_WAIT(wakeupDriver);		SWAP;	// goto sleep
		if (mySEM_TRYLOCK(needDriver))			// i’m awake  - driver needed?
		{					// yes
			driverDoneSemaphore = driverDone;		SWAP;	// pass notification semaphore
			mySEM_SIGNAL(driverReady);		SWAP;	// driver is awake
			mySEM_WAIT(carReady);		SWAP;	// wait for car ready to go
			mySEM_WAIT(driverDone);		SWAP;	// drive ride
		}
		else if (mySEM_TRYLOCK(needTicket))			// someone need ticket?
		{					// yes
			mySEM_WAIT(tickets);		SWAP;	// wait for ticket (counting)
			mySEM_SIGNAL(takeTicket);		SWAP;	// print a ticket (binary)
		}
		else break;			// don’t bother me!
	}
	*/
	return 0;
}

int jurassic_car(int argc, char* argv[]) {
	int carID = INTEGER(argv[1]);
	int i;
	char buf[32];
	Semaphore* passengers[3];
	Semaphore* driver;

	// Start up the infinite car
	while(1) {

		// Make sure it gets loaded with 3 passengers + driver
		for(i=0; i < NUM_SEATS; i++) {
			// Wait for request to fill seat
			SEM_WAIT(fillSeat[carID]);				SWAP
			loadingCar = carID;		SWAP;
			SEM_SIGNAL(getPassenger);		SWAP;


			// Signal the passenger that we're ready for them
			//SEM_SIGNAL(getPassenger);					SWAP

			// Wait for Passenger to claim seat
			SEM_WAIT(seatTaken);							SWAP

			// Passenger is ready
			//SEM_SIGNAL(passengerSeated);			SWAP

			// Is the car full?
			if( i == NUM_SEATS-1 )
			{
				// Wait for driver to be non-busy
				// SEM_WAIT(needDriver);			SWAP

				// Driver is available, Signal he needs to drive
				SEM_SIGNAL(needDriver);		SWAP;
				SEM_SIGNAL(wakeupDriver);				SWAP
				SEM_WAIT(driverLoaded);		SWAP;
				driver = loadingDriver;		SWAP;

				// Signal that the car no longer needs driver
				//SEM_SIGNAL(needDriver);		SWAP
			}

			passengers[i] = loadingPassenger;		SWAP;

			SEM_WAIT(parkMutex);		SWAP;
			myPark.numInCars++;		SWAP;
			myPark.numInCarLine--;		SWAP;
			SEM_SIGNAL(parkMutex);		SWAP;

			// Signal that a seat has been filled
			// If this is the last passenger, the car will also
			// have a driver at this point
			SEM_SIGNAL(seatFilled[carID]);			SWAP
		}

		// Wait for the ride to get over. The car should leave!
		SEM_WAIT(rideOver[carID]);            SWAP;

		for(i=0; i < NUM_SEATS; i++) {
			SEM_SIGNAL(passengers[i]);		SWAP;
		}

		SEM_SIGNAL(driver);		SWAP;

	}

	return 0;
}



// ***********************************************************************
// ***********************************************************************
// delta clock command
int P3_dc(int argc, char* argv[])
{
	int i = 0;
	printf("DeltaClock\n----------\n");
	for(i=0; i<deltaClockSize; i++) {
		printf("\n%i: %i seconds - %s", i, dc[i].time, dc[i].sem->name);
	}
	return 0;
} // end CL3_dc

/*
// ***********************************************************************
// ***********************************************************************
// ***********************************************************************
// ***********************************************************************
// ***********************************************************************
// ***********************************************************************
// delta clock command
int P3_dc(int argc, char* argv[])
{
	printf("\nDelta Clock");
	// ?? Implement a routine to display the current delta clock contents
	//printf("\nTo Be Implemented!");
	int i;
	for (i=0; i<numDeltaClock; i++)
	{
		printf("\n%4d%4d  %-20s", i, deltaClock[i].time, deltaClock[i].sem->name);
	}
	return 0;
} // end CL3_dc


// ***********************************************************************
// display all pending events in the delta clock list
void printDeltaClock(void)
{
	int i;
	for (i=0; i<numDeltaClock; i++)
	{
		printf("\n%4d%4d  %-20s", i, deltaClock[i].time, deltaClock[i].sem->name);
	}
	return;
}


// ***********************************************************************
// test delta clock
int P3_tdc(int argc, char* argv[])
{
	createTask( "DC Test",			// task name
		dcMonitorTask,		// task
		10,					// task priority
		argc,					// task arguments
		argv);

	timeTaskID = createTask( "Time",		// task name
		timeTask,	// task
		10,			// task priority
		argc,			// task arguments
		argv);
	return 0;
} // end P3_tdc



// ***********************************************************************
// monitor the delta clock task
int dcMonitorTask(int argc, char* argv[])
{
	int i, flg;
	char buf[32];
	// create some test times for event[0-9]
	int ttime[10] = {
		90, 300, 50, 170, 340, 300, 50, 300, 40, 110	};

	for (i=0; i<10; i++)
	{
		sprintf(buf, "event[%d]", i);
		event[i] = createSemaphore(buf, BINARY, 0);
		insertDeltaClock(ttime[i], event[i]);
	}
	printDeltaClock();

	while (numDeltaClock > 0)
	{
		SEM_WAIT(dcChange)
		flg = 0;
		for (i=0; i<10; i++)
		{
			if (event[i]->state ==1)			{
					printf("\n  event[%d] signaled", i);
					event[i]->state = 0;
					flg = 1;
				}
		}
		if (flg) printDeltaClock();
	}
	printf("\nNo more events in Delta Clock");

	// kill dcMonitorTask
	tcb[timeTaskID].state = S_EXIT;
	return 0;
} // end dcMonitorTask
*/

extern Semaphore* tics1sec;

// ********************************************************************************************
// display time every tics1sec
int timeTask(int argc, char* argv[])
{
	char svtime[64];						// ascii current time
	while (1)
	{
		SEM_WAIT(tics1sec)
		printf("\nTime = %s", myTime(svtime));
	}
	return 0;
} // end timeTask
