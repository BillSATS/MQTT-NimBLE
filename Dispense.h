#pragma once
//class Dispense (int numberCycles)  //abstract class
//{
//public:
//	virtual void DispenseTreat() = 0;
//};
//


#ifndef _DISPENSE_h
#define _DISPENSE_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "arduino.h"
#else
#include "WProgram.h"
#endif


/*function declarations*/
void FeederStateMachine();
void BlinkLed();
void ProcessClientCmd(char cmd);

/*feederState*/
#define FeedStopped	0
#define SingleFeed	1
#define AutoFeed	2
#define JackpotFeed	3
/*feederType*/
#define UNO			1
#define MINI		2
/*ESP32 GPIO*/
#define LED			2

extern int	FeedState;// = FeedStopped;
extern int	feederType;//default is UNO but this can be changed via the BLE connection
extern int	FeedsPerJackpot;//this will be set by user at some point
extern int	FeedCount;//init to max feeds per Jackpot

#endif


//Keep ProcessClientCmd short to let the callback run. instead change the feeder state flag
//void ProcessClientCmd(char cmd);


#ifdef timerdidnotcompile
/* ESP32 hardware timer declarations*/
volatile int count;
int totalInterrupts;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
// Code with critica section
void IRAM_ATTR onTime() {
	portENTER_CRITICAL_ISR(&timerMux);
	count++;
	portEXIT_CRITICAL_ISR(&timerMux);
}

// Code without critical section
/*void IRAM_ATTR onTime() {
   count++;
}*/
/*end of ESP hardware timer */
#endif
