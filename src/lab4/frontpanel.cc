/*!***************************************************************************
*!
*! FILE NAME  : FrontPanel.cc
*!
*! DESCRIPTION: Handles the LED:s
*!
*!***************************************************************************/

/****************** INCLUDE FILES SECTION ***********************************/

#include "compiler.h"

#include "iostream.hh"
#include "frontpanel.hh"

//#define D_FP
#ifdef D_FP
#define trace cout
#else
#define trace if(false) cout
#endif

/****************** FrontPanel DEFINITION SECTION ***************************/

//----------------------------------------------------------------------------
//



/****************** LED CLASS ***************************/

LED::LED(byte theLedNumber) : myLedBit(theLedNumber)	// Constructor: initiate the bitmask 'myLedBit'.
{
}

static byte write_out_register_shadow = 0x78;

void
LED::on() {	// turn this led on
	uword led = 4 << myLedBit;  /* convert LED number to bit weight */
	write_out_register_shadow &= ~led;
	*(VOLATILE byte*)0x80000000 = write_out_register_shadow;

	iAmOn = true;
}
void
LED::off() {	// turn this led off
	uword led = 4 << myLedBit;  /* convert LED number to bit weight */
	write_out_register_shadow |= led;
	*(VOLATILE byte*)0x80000000 = write_out_register_shadow;

	iAmOn = false;
}
void
LED::toggle() {	// toggle this led

	if (iAmOn) {
		off();
	}
	else {
		on();
	}

}


/****************** NetworkLEDTimer CLASS ***************************/


NetworkLEDTimer::NetworkLEDTimer(Duration blinkTime) : myBlinkTime(blinkTime) {	// Constructor: initiate myBlinkTime

}
void
NetworkLEDTimer::start() {	// Start timer
	this->timeOutAfter(myBlinkTime);
	
	
}

void
NetworkLEDTimer::timeOut() {	// notify FrontPanel that this timer has expired.
	FrontPanel::instance().notifyLedEvent(FrontPanel::networkLedId);
}



/****************** CDLEDTimer CLASS ***************************/
CDLEDTimer::CDLEDTimer(Duration blinkPeriod) {	// Constructor: initiate and start timer
	this->timerInterval(blinkPeriod);
	this->startPeriodicTimer();
}

void
CDLEDTimer::timerNotify() {	// notify FrontPanel that this timer has expired.
	FrontPanel::instance().notifyLedEvent(FrontPanel::cdLedId);
}


/****************** StatusLEDTimer CLASS ***************************/
StatusLEDTimer::StatusLEDTimer(Duration blinkPeriod) {	// Constructor: initiate and start timer
	this->timerInterval(blinkPeriod);
	this->startPeriodicTimer();
}

void
StatusLEDTimer::timerNotify() {	// notify FrontPanel that this timer has expired.
	FrontPanel::instance().notifyLedEvent(FrontPanel::statusLedId);
}


/****************** FrontPanel CLASS ***************************/

FrontPanel::FrontPanel() :	// Constructor: initializes the semaphore the leds and the event flags.
	Job(),
	mySemaphore(Semaphore::createQueueSemaphore("FP", 0)),
	myNetworkLED(networkLedId),
	myCDLED(cdLedId),
	myStatusLED(statusLedId)
{	 
	cout << "FrontPanel created." << endl;
	Job::schedule(this);

}

FrontPanel&
FrontPanel::instance() {	// Returns the instance of FrontPanel, used for accessing the FrontPanel
	
	/*FrontPanel frontPanel;
	if (frontPanel == null) {
		frontPanel = FrontPanel.FrontPanel();
	}
	return frontPanel;*/

	static FrontPanel myInstance;
	return myInstance;
}

void
FrontPanel::packetReceived() {	// turn Network led on and start network led timer
	myNetworkLED.on();
	myNetworkLEDTimer->start();
	
}

void
FrontPanel::notifyLedEvent(uword theLedId) {	// Called from the timers to notify that a timer has expired.
												// Sets an event flag and signals the semaphore.

	if (theLedId == networkLedId) {
		netLedEvent = true;
	}

	if (theLedId == cdLedId) {
		cdLedEvent = true;
	}

	if (theLedId == statusLedId) {
		statusLedEvent = true;
	}
	mySemaphore->signal();


}

void
FrontPanel::doit() {	// Main thread loop of FrontPanel. Initializes the led timers and goes into
						// a perptual loop where it awaits the semaphore. When it wakes it checks 
						// the event flags to see which leds to manipulate and manipulates them. 


	myNetworkLEDTimer = new NetworkLEDTimer(Clock::seconds * 1); //Timer 1 secs
	myCDLEDTimer = new CDLEDTimer(Clock::seconds * 1); //Timer 1 secs
	myStatusLEDTimer = new StatusLEDTimer(Clock::seconds * 2); //Timer 2 secs

	//NetworkLED on for 10 secs
	//packetReceived();
	//cout << "Turn on NetworkLED" << endl;

	while (true) {
		mySemaphore->wait();
		if (statusLedEvent) {
			myStatusLED.toggle();
			//cout << "Toggle StatusLED" << endl;
			statusLedEvent = false;
		}
		if (cdLedEvent) {
			myCDLED.toggle();
			//cout << "Toggle CDLED" << endl;
			cdLedEvent = false;
		}
		if (netLedEvent) {
			myNetworkLED.off();
			netLedEvent = false;
		}

	}


}


/****************** END OF FILE FrontPanel.cc ********************************/

