
// 
// 
// 
// 
#pragma once

#include "PTStepper.h"
#include "Dispense.h"

/******************stepper declarations******************************/
int ourSteps(0);
int targetSteps;
int lastType = 0;
// 
////Put all the pins in an array to make them easy to work with
int pins[]{
    13,  //IN1 on the ULN2003 Board, BLUE end of the Blue/Yellow motor coil
    12,  //IN2 on the ULN2003 Board, PINK end of the Pink/Orange motor coil
    14,  //IN3 on the ULN2003 Board, YELLOW end of the Blue/Yellow motor coil
    27   //IN4 on the ULN2003 Board, ORANGE end of the Pink/Orange motor coil
};

//Define the full step sequence.  
//With the pin (coil) states as an array of arrays
int fullStepCount = 4;
int fullSteps[][4] = {
    {HIGH,HIGH,LOW,LOW},
    {LOW,HIGH,HIGH,LOW},
    {LOW,LOW,HIGH,HIGH},
    {HIGH,LOW,LOW,HIGH}
};

//Keeps track of the current step.
//We'll use a zero based index. 
int currentStep = 0;
int cycleCnt = 0;
int cycleCounter = 0;

//Keeps track of the current direction
//Relative to the face of the motor. 
//Clockwise (true) or Counterclockwise(false)
//We'll default to clockwise
bool clockwise = true;


void SetupStepper();
void cycle();
void step(int steps[][4], int stepCount);
/****************  end stepper declarations  *******************************************************************/

/***************** begin stepper actions     *******************************************************************/

/************* Set all motor pins off which turns off the motor ************************************************/
void ClearPins() {
    for (int pin = 0; pin < 4; pin++) {
        pinMode(pins[pin], OUTPUT);
        digitalWrite(pins[pin], LOW);
    }

}
void whoAreWe() {
    // Define number of steps per rotation:
    // Calculate how many micro-steps to complete one feed sb 128 for Uno or 512 for Mini
/********************************************************************************************** Comment out for test ***********************************************    
    if (feederType == UNO)            // Uno makes 16 steps per revolution
        ourSteps = 16;
    else if (feederType == MINI)     // Mini makes 4 steps per revolution but reverses
        ourSteps = 4;
    else
        ourSteps = 2048;            // Fall thru handling

    targetSteps = 2048 / ourSteps;
    lastType = feederType;          // let's not do this again until needed
*********************************************************************************** End original, Begin test *************************************************************************************/    
    if (feederType == UNO)            // Uno makes 16 steps per revolution
        targetSteps = 130;
    else if (feederType == MINI)     // Mini makes 4 steps per revolution but reverses
        targetSteps = 512;
    else
        targetSteps = 2048;            // Fall thru handling


    lastType = feederType;          // let's not do this again until needed
/*********************************************************************************** End test ************************************************************************/
    
}

//Prepare motor controller
void SetupStepper() {
    ClearPins();                     // Go turn all motor pins off
    //Serial.print("ourSteps after  ");
    //Serial.print(ourSteps);
    //Serial.println(" ");
}

void step(int steps[][4], int stepCount) {
    //Then we can figure out what our current step within the sequence from the overall current step
    //and the number of steps in the sequence
    //Serial.print("current step ");
    //Serial.println(currentStep);
    int currentStepInSequence = currentStep % stepCount;

    //Figure out which step to use. If clock wise, it is the same is the current step
    //if not clockwise, we fire them in the reverse order...
    int directionStep = clockwise ? currentStepInSequence : (stepCount - 1) - currentStepInSequence;

    //Set the four pins to their proper state for the current step in the sequence, 
    //and for the current direction
    for (int pin = 0; pin < 4; pin++) {
        digitalWrite(pins[pin], steps[directionStep][pin]);
    }
}

void cycle() {
    //Get a local reference to the number of steps in the sequence
    //And call the step method to advance the motor in the proper direction

    //Full Step
    int stepCount = fullStepCount;
    step(fullSteps, fullStepCount);

    // Increment the program field tracking the current step we are on
    ++currentStep;

    // If targetSteps has been specified, and we have reached
    // that number of steps, reset the currentStep, and reverse directions
    if (targetSteps != 0 && currentStep == targetSteps) {
        currentStep = 0;
        ClearPins(); // Turn off motor
    }
    else if (targetSteps == 0 && currentStep == stepCount) {
        // don't reverse direction, just reset the currentStep to 0
        // resetting this will prevent currentStep from 
        // eventually overflowing the int variable it is stored in.
        currentStep = 0;
        ClearPins();
    }

    //2 milliseconds seems to be about the shortest delay that is usable.  Anything
    //lower and the motor starts to freeze. 
    delay(2);
}

//This will advance the stepper clockwise once by the angle specified in SetupStepper. Example 16 pockets in UNO is 22.5 degrees
void StartStepper() {
    if (ourSteps == 0 || feederType != lastType)              // Not assigned yet, is zero or is different type
        whoAreWe();                                           // go figure who we are, Mini or Uno and calculate steps needed                                                               
    Serial.println("**************** Starting Stepper *******************");
/***************************************** Testing stepper ********************
Serial.print("cycleCounter ");
Serial.println(cycleCounter);
Serial.print("targetSteps  ");
Serial.println(targetSteps);
delay(20);
****************************************** end testing code *********************/
    while (cycleCounter < targetSteps) {
        // Step one unit in forward 
        cycleCounter++;
        cycle();
    }
    cycleCounter = 0;       // Reset when done
    Serial.println("**************** Ending Stepper *************");


    if (feederType == MINI) {
        Serial.println("**************** Starting Return *******************");
        clockwise = !clockwise;     // Time to go backwards
        while (cycleCounter < targetSteps - 5) {        //Don't quite go all the way back
            // Step one unit backwards
            cycleCounter++;
            cycle();
        }
        ClearPins();
        cycleCounter = 0;           // Reset when done
        clockwise = !clockwise;     // next time go forward
        Serial.println("**************** Emding Return ***********");

    }

}
