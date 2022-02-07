#pragma once

#include "Dispense.h"

int FeedState = FeedStopped;
int	feederType = UNO; //default is UNO but this can be changed via the BLE connection
int FeedsPerJackpot = 16; //this will be set by user at some point
int FeedCount = FeedsPerJackpot; //init to max feeds per Jackpot

#if 1 // buildgood30AUG2021


#include "PTStepper.h"

////Keep ProcessClientCmd short to let the callback run. instead change the feeder state flag
void ProcessClientCmd(char cmd) {
    if ((cmd == 0x00) || (cmd == 's') || (cmd == 'c'))  FeedState = SingleFeed;  // Gen 3 Pet Tutor used 0x00 to feed so 0x00 is to make it backward compatible  's' is the new version
    else if (cmd == 'a') FeedState = AutoFeed;
    else if (cmd == 'j') FeedState = JackpotFeed;
    else if (cmd == 'u') feederType = UNO;
    else if (cmd == 'm') feederType = MINI;
    else
    {
        Serial.println("*****invalid command from client*****");
    }
}

void FeederStateMachine() {
    switch (FeedState)  // FeedState is public and can be set by BLE client via the GATT for the characteristic
    {
    case SingleFeed:
    {
        StartStepper();
        BlinkLed();
        Serial.println("SINGLE Feed"); //this should write a 0x01 for feed ack
        FeedState = FeedStopped; //this will cancel Continous feed if it is set
        break;
    }
    case AutoFeed:  //note: the only difference in this case is the FeedStopped does NOT get set
    {
        if (1 == 1) {  //?? change this to check a timer to see when to fire the feeder..session length,random interval, auto increment time
            StartStepper();
            BlinkLed();
            Serial.println("AUTO Feed");
        }
        break;
    }
    case JackpotFeed:  //note: the only difference in this case is the FeedStopped does NOT get set
    {
        if (FeedCount > 0) {
            if (1 == 1) {  //?? change this to check a timer to see when to fire the feeder
                StartStepper();
                BlinkLed();
                Serial.println("JACKPOT Feed");
                FeedCount--;
            }
        }
        else {
            FeedCount = FeedsPerJackpot; //reset counter after all feeds for Jackpot completed
            FeedState = FeedStopped;
        }
        break;
    }
    case FeedStopped:
    {
        //Serial.println("Feed Stopped --  ");
        //do nothing
        break;
    }
    default:
    {
        FeedState = FeedStopped;
        break;
    }
    }
};

void BlinkLed() {
    digitalWrite(LED, HIGH);
    delay(300);
    digitalWrite(LED, LOW);//
}

#endif
