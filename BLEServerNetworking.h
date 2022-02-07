//
//  BLENetworking.hpp
//  M5Stick
//
//  Created by Scott Moody on 1/19/22.
//

#ifndef BLEServerNetworking_h
#define BLEServerNetworking_h

//!defines the operations on BLE Server Networking
//!BLEServerNetworking is the "server" side of the BLE interface

//!the 'setup' for this module BLEServerNetworking. Here the service name is added (and potentially more later)
void setupBLEServerNetworking(char *serviceName, char * deviceName, char *serviceUUID, char *characteristicUUID);
#define SERVICE_UUID        "B0E6A4BF-CCCC-FFFF-330C-0000000000F0"  //Pet Tutor feeder service for feed
#define CHARACTERISTIC_UUID "B0E6A4BF-CCCC-FFFF-330C-0000000000F1"  //Pet Tutor feeder characteristic

//!the 'setup' for this module BLEServerNetworking. Here the service name is added (and potentially more later)
void loopBLEServerNetworking();

#define CALLBACK_ONREAD 0
#define CALLBACK_ONWRITE 1
#define MAX_CALLBACKS 2

//! register the callback function (with a char* message) with the callbackType (1,2,3...)
//!  eg  registerCallback(CALLBACK_FEED, &messageFeedFunction)
void registerCallbackBLEServer(int callbackType, void (*callback)(char*));

//!sets the device name
void setBLEServerDeviceName(char *deviceName);

//!send something over bluetooth, this right now is 0x01 
void sendBLEMessageACKMessage();

#endif /* BLEServerNetworking_hpp */
