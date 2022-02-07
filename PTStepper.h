// PTStepper.h

#ifndef _PTSTEPPER_h
#define _PTSTEPPER_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif



/* function declarations*/

void StartStepper();

void SetupStepper();

#endif

