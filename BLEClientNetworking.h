//
//  BLEClientNetworking.hpp
//  M5Stick
//
//  Created by Scott Moody on 1/20/22.
//

#ifndef BLEClientNetworking_h
#define BLEClientNetworking_h

//#include <stdio.h>

//!These are the legacy PetTutor known addresses..
#define SERVICE_UUID        "B0E6A4BF-CCCC-FFFF-330C-0000000000F0"  //Pet Tutor feeder service for feed
#define CHARACTERISTIC_UUID "B0E6A4BF-CCCC-FFFF-330C-0000000000F1"  //Pet Tutor feeder characteristic


#define CALLBACK_ONREAD 0
#define CALLBACK_ONWRITE 1
#define CALLBACK_BLINK_LIGHT 2
#define MAX_CALLBACKS 3

//! register the callback function (with a char* message) with the callbackType (1,2,3...)
//!  eg  registerCallback(CALLBACK_FEED, &messageFeedFunction)
void registerCallbackBLEClient(int callbackType, void (*callback)(char*));

//!the 'setup' for this module BLEClientNetworking. Here the service name is added (and potentially more later)
void setupBLEClientNetworking(char *serviceName, char *serviceUUID, char *characteristicUUID);

//!the loop()
void loopBLEClientNetworking();

//FOR NOW THIS IS HERE.. but it should be more generic. eg:  sendBluetoothCommand() ..
//send the feed command
void sendFeedCommandBLEClient();

#endif /* BLEClientNetworking_h */
