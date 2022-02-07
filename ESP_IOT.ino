/*
    Name:		NimBLE_PetTutor_Server.ino
    Created:	8/25/2021 7:49:11 PM
    Author:	wes
        added stepper and dispense modules 9-4-21 wha/WJL

    nimconfig.h can change the max clients allowed(up to 9). default is 3  wha
   added to new repo NimBLE ...
*/

/** NimBLE_Server Demo:

    Demonstrates many of the available features of the NimBLE server library.
r
    Created: on March 22 2020
        Author: H2zero

*/
#define VERSION "Version 1.2 01.20.2022"
#include "PTStepper.h"
#include "Dispense.h"

/*******************************MQTT*************************************/
//!added 1.1.2022 by iDogWatch.com
#define MQTT_NETWORKING
#ifdef MQTT_NETWORKING
#include "MQTTNetworking.h"
#endif

#define USE_BLE_SERVER_NETWORKING
#ifdef  USE_BLE_SERVER_NETWORKING
#include "BLEServerNetworking.h"

//*** The callback for "onWrite" of the bluetooth "onWrite'
void onWriteBLEServerCallback(char *message);

#else   //NOT USE_BLE_SERVER_NETWORKING (inline)

#endif  //not BLEServerNetworking


#ifdef MQTT_NETWORKING
//!example callback: but the scope would have the pCharacteristic defined, etc..
//!This is pased just before the setupMQTTNetworking() is called..
void feedMessageCallback(char *message)
{
  Serial.printf("\nNimBLE_PetTutor_Server.messageCallback: %s\n", message);
  char * rxValue = (char*)"c";  //feed

  ProcessClientCmd(rxValue[0]);   //?? client sent a command now see what it is
  // pCharacteristic->setValue(0x01);  //??  This is the acknowlege(ACK) back to client.  Later this should be contigent on a feed completed
}

//!callback for blinking led
void blinkMessageCallback(char *message)
{
  //call the already defined blink led
  BlinkLed();
}
#endif

#define PT_SERVICE_UUID        "B0E6A4BF-CCCC-FFFF-330C-0000000000F0"  //Pet Tutor feeder service for feed
#define PT_CHARACTERISTIC_UUID "B0E6A4BF-CCCC-FFFF-330C-0000000000F1"  //Pet Tutor feeder characteristic

//!main setup
void setup() {
  Serial.begin(115200);
  pinMode(LED, OUTPUT);

  Serial.println("******************");
  Serial.println(VERSION);
  Serial.println("******************");

  //setup the stepper for ther feeder
  SetupStepper();

  FeedState = FeedStopped;
  Serial.println("Starting NimBLE_PetTutor_Server");

#ifdef MQTT_NETWORKING

  //pass the callbacks, (1) the message callback, and the (blinkLed) implementation
  //setMessageCallbacks(&feedMessageCallback, &blinkMessageCallback);

  //register the 2 callbacks for now..
  registerCallback(CALLBACK_FEED, &feedMessageCallback);
  registerCallback(CALLBACK_BLINK, &blinkMessageCallback);

  //perform the setup() in the MQTTNetworking
  setupMQTTNetworking();

  // sets the _chipName
  char *deviceName = getDeviceName();

  //THIS should output the device name, user name, etc...
  //Serial.print("pFoodCharacteristic->setValue: ");
  //Serial.println(_fullJSONString);
  Serial.println(deviceName);

#endif  //MQTT_NETWORKING

#ifdef USE_BLE_SERVER_NETWORKING   //yes
    
    //Initialize the BLE Server, using the service name PTFeeder
    //https://stackoverflow.com/questions/20944784/why-is-conversion-from-string-constant-to-char-valid-in-c-but-invalid-in-c
#ifdef MQTT_NETWORKING
    //*** The callback for "onWrite" of the bluetooth "onWrite'
    registerCallbackBLEServer(CALLBACK_ONWRITE, &onWriteBLEServerCallback);
    //strdup() get away from the
    setupBLEServerNetworking(strdup("PTFeeder"), getDeviceName(), strdup(PT_SERVICE_UUID), strdup(PT_CHARACTERISTIC_UUID));

#else
  setupBLEServerNetworking(strdup("PTFeeder"), strdup("undefinedDevice"), strdup(PT_SERVICE_UUID), strdup(PT_CHARACTERISTIC_UUID));
#endif


#endif // USE_BLE_SERVER_NETWORKING
}


void loop() {
  /** Do your thing here, this just spams notifications to all connected clients */
  FeederStateMachine(); //??

  /*******************************MQTT*************************************/
#ifdef MQTT_NETWORKING
  loopMQTTNetworking();
#endif

#ifdef USE_BLE_SERVER_NETWORKING
  loopBLEServerNetworking();
#else
#endif // USE_BLE_SERVER_NETWORKING

  delay(100);  //this delays the feed trigger response

}




//*** The callback for "onWrite" of the bluetooth "onWrite'
void onWriteBLEServerCallback(char *message)
{
  Serial.print("onWriteBLEServerCallback: ");
  Serial.println(message);

#ifdef MQTT_NETWORKING
  //This should be a 'register' command..

  //the MQTTNetwork.processJSONMessage()
  // will return true if the message was JSON and was processes, false otherwise.
  if (processJSONMessage(message))
  {
    //processed by the MQTTNetworking code..
#ifdef USE_BLE_SERVER_NETWORKING
    sendBLEMessageACKMessage();
#endif
//pCharacteristic->setValue(0x01);  //??  This is the acknowlege(ACK) back to client.  Later this should be contigent on a feed completed
  }
  else
  {
    //perform feed...
    Serial.println("Perform FEED");

    //std::string rxValue = pCharacteristic->getValue().c_str();  //??
    //call Dispence.cpp's
    ProcessClientCmd(message[0]);   //?? client sent a command now see what it is
#ifdef USE_BLE_SERVER_NETWORKING
    sendBLEMessageACKMessage();
#endif
    //ProcessClientCmd(rxValue[0]);   //?? client sent a command now see what it is

    // pCharacteristic->setValue(0x01);  //??  This is the acknowlege(ACK) back to client.  Later this should be contigent on a feed completed

  }
  Serial.println("*********");

#endif
} //onWriteBLEServerCallback
