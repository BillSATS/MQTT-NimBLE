//
//  BLEClientNetworking.cpp
//  M5Stick
//
//  Created by Scott Moody on 1/20/22.
//

#include "BLEClientNetworking.h"

#include <NimBLEDevice.h>
#include <Arduino.h>



//************* BLEClient Callback Registrations **************************/
//!performs the indirect callback based on the callbackType
void callCallbackBLEClient(int callbackType, char *message);

//!example callback: but the scope would have the pCharacteristic defined, etc..
void dummyBLEClientCallback(char *message)
{
    Serial.println("*** No callback defined ..");
}


//!define as: void callback(char* message)
//!  call processMessage(message, &callback);
//!the array of callback functions
void (*_callbackFunctionsBLEClient[MAX_CALLBACKS])(char *);
//!flag for initializing if not yes
int _callbacksInitializedBLEClient = false;
//!init the callbacks to dummy callbacks
void initCallbacksBLEClient()
{
    for (int i = 0; i < MAX_CALLBACKS; i++)
    {
        _callbackFunctionsBLEClient[i] = &dummyBLEClientCallback;
    }
}

//!register the callback based on the callbackType
void registerCallbackBLEClient(int callbackType, void (*callback)(char*))
{
    if (!_callbacksInitializedBLEClient)
    {
        _callbacksInitializedBLEClient = true;
        initCallbacksBLEClient();
    }
    if (callbackType < 0 || callbackType >= MAX_CALLBACKS)
    {
        Serial.println("#### Error outside callback range");
    }
    else
    {
        _callbackFunctionsBLEClient[callbackType] = callback;
        Serial.printf("registerCallbackBLEClient(%d)\n", callbackType);
    }
}

//!performs the indirect callback based on the callbackType
void callCallbackBLEClient(int callbackType, char *message)
{
    if (callbackType < 0 || callbackType >= MAX_CALLBACKS)
    {
        Serial.println("#### Error outside callback range");
    }
    else
    {
      Serial.printf("callCallbackBLEClient(%d,%s)\n", callbackType, message);
        void (*callbackFunctionsBLEClient)(char *) = _callbackFunctionsBLEClient[callbackType];
        (*callbackFunctionsBLEClient)(message);
    }
}


//**** NIM BLE Client Code

//!need to device way to change these ...

//  uses  NimBLEUUID

static NimBLEUUID _BLEClientServiceUUID("B0E6A4BF-CCCC-FFFF-330C-0000000000F0"); //??
static NimBLEUUID _BLEClientCharacteristicUUID("b0e6a4bf-cccc-ffff-330c-0000000000f1");
static NimBLERemoteCharacteristic* _BLEClientCharacteristicFeed;

//forward called on the end of the scan
void scanEndedCB_BLEClient(NimBLEScanResults results);

static NimBLEAdvertisedDevice* _advertisedDevice;
static uint32_t _scanTime = 0; /** 0 = scan forever */
static bool _doConnect = false;

//bool FeedFlag = false;

/**  None of these are required as they will be handled by the library with defaults. **
 **                       Remove as you see fit for your needs                        */
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) {
        Serial.println("Connected");
        /** After connection we should change the parameters if we don't need fast response times.
         *  These settings are 150ms interval, 0 latency, 450ms timout.
         *  Timeout should be a multiple of the interval, minimum is 100ms.
         *  I find a multiple of 3-5 * the interval works best for quick response/reconnect.
         *  Min interval: 120 * 1.25ms = 150, Max interval: 120 * 1.25ms = 150, 0 latency, 60 * 10ms = 600ms timeout
         */
        pClient->updateConnParams(120, 120, 0, 60);
    };
    
    void onDisconnect(NimBLEClient* pClient) {
        Serial.print(pClient->getPeerAddress().toString().c_str());
        Serial.println(" Disconnected - Starting scan");
        NimBLEDevice::getScan()->start(_scanTime, scanEndedCB_BLEClient);
    };
    
    /** Called when the peripheral requests a change to the connection parameters.
     *  Return true to accept and apply them or false to reject and keep
     *  the currently used parameters. Default will return true.
     */
    bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params) {
        if (params->itvl_min < 24) { /** 1.25ms units */
            return false;
        }
        else if (params->itvl_max > 40) { /** 1.25ms units */
            return false;
        }
        else if (params->latency > 2) { /** Number of intervals allowed to skip */
            return false;
        }
        else if (params->supervision_timeout > 100) { /** 10ms units */
            return false;
        }
        
        return true;
    };
    
    /********************* Security handled here **********************
     ****** Note: these are the same return values as defaults ********/
    uint32_t onPassKeyRequest() {
        Serial.println("Client Passkey Request");
        /** return the passkey to send to the server */
        return 123456;
    };
    
    bool onConfirmPIN(uint32_t pass_key) {
        Serial.print("The passkey YES/NO number: ");
        Serial.println(pass_key);
        /** Return false if passkeys don't match. */
        return true;
    };
    
    /** Pairing process complete, we can check the results in ble_gap_conn_desc */
    void onAuthenticationComplete(ble_gap_conn_desc* desc) {
        if (!desc->sec_state.encrypted) {
            Serial.println("Encrypt connection failed - disconnecting");
            /** Find the client with the connection handle provided in desc */
            NimBLEDevice::getClientByID(desc->conn_handle)->disconnect();
            return;
        }
    };
};


/** Define a class to handle the callbacks when advertisments are received */
class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        Serial.print("Advertised Device found: ");
        Serial.println(advertisedDevice->toString().c_str());
        String deviceInfo = advertisedDevice->toString().c_str();
        if (advertisedDevice->isAdvertisingService(NimBLEUUID(_BLEClientServiceUUID))  || (deviceInfo.indexOf("b0e6a4bf-cccc-ffff-330c-0000000000f0") > 0)) //this was the original code service called "DEAD"
        {
            Serial.println("Found Our Service");
            /** stop scan before connecting */
            NimBLEDevice::getScan()->stop();
            /** Save the device reference in a global for the client to use*/
            _advertisedDevice = advertisedDevice;
            /** Ready to connect now */
            _doConnect = true;
        }
    };
};


/** Notification / Indication receiving handler callback */
void notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    std::string str = (isNotify == true) ? "Notification" : "Indication";
    str += " from ";
    /** NimBLEAddress and NimBLEUUID have std::string operators */
    str += std::string(pRemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress());
    str += ": Service = " + std::string(pRemoteCharacteristic->getRemoteService()->getUUID());
    str += ", Characteristic = " + std::string(pRemoteCharacteristic->getUUID());
    str += ", Value = " + std::string((char*)pData, length);
    Serial.println(str.c_str());
}

/** Callback to process the results of the last scan or restart it */
void scanEndedCB_BLEClient(NimBLEScanResults results) {
    Serial.println("Scan Ended");
}

/** Create a single global instance of the callback class to be used by all clients */
static ClientCallbacks _clientCB_BLEClient;

/** Handles the provisioning of clients and connects / interfaces with the server */
bool connectToServer_BLEClient()
{
    NimBLEClient* pClient = nullptr; //creates a new instance of a pointer to a client(1 for each server it connects to)
    /** Check if we have a client we should reuse first **/
    if (NimBLEDevice::getClientListSize()) {  //this runs only if #clients>= 1...ie no client instance start yet
        /** Special case when we already know this device, we send false as the
         *  second argument in connect() to prevent refreshing the service database.
         *  This saves considerable time and power.
         */
        pClient = NimBLEDevice::getClientByPeerAddress(_advertisedDevice->getAddress()); //if the server was connected to before there should be an address stored for it -wha
        if (pClient) {
            if (!pClient->connect(_advertisedDevice, false)) {
                Serial.println("Reconnect failed");
                return false;
            }
            Serial.println("Reconnected client");
        }
        /** We don't already have a client that knows this device,
         *  we will check for a client that is disconnected that we can use.
         */
        else {
            pClient = NimBLEDevice::getDisconnectedClient(); //reusing a client that was created but is disconnected  -wha
        }
    }
    
    /** No client to reuse? Create a new one. */
    if (!pClient) { //if a client instance is created from above this will not be executed -wha
        if (NimBLEDevice::getClientListSize() >= 1 ) {   //wha -original example code used the max configuration of 3 --> NIMBLE_MAX_CONNECTIONS) {
            Serial.println("Max clients reached - no more connections available");
            return false;
        }
        
        pClient = NimBLEDevice::createClient();
        
        Serial.println("New client created");
        
        pClient->setClientCallbacks(&_clientCB_BLEClient, false);
        /** Set initial connection parameters: These settings are 15ms interval, 0 latency, 120ms timout.
         *  These settings are safe for 3 clients to connect reliably, can go faster if you have less
         *  connections. Timeout should be a multiple of the interval, minimum is 100ms.
         *  Min interval: 12 * 1.25ms = 15, Max interval: 12 * 1.25ms = 15, 0 latency, 51 * 10ms = 510ms timeout
         */
        pClient->setConnectionParams(12, 12, 0, 51);
        /** Set how long we are willing to wait for the connection to complete (seconds), default is 30. */
        pClient->setConnectTimeout(5);
        
        
        if (!pClient->connect(_advertisedDevice)) {
            /** Created a client but failed to connect, don't need to keep it as it has no data */
            NimBLEDevice::deleteClient(pClient);
            Serial.println("Failed to connect, deleted client");
            return false;
        }
    }
    
    if (!pClient->isConnected()) {
        if (!pClient->connect(_advertisedDevice)) {
            Serial.println("Failed to connect");
            return false;
        }
    }
    Serial.printf("number clients=   %d \n\r", NimBLEDevice::getClientListSize()); //
    Serial.print("Connected to: ");
    Serial.println(pClient->getPeerAddress().toString().c_str());
    Serial.print("RSSI: ");
    Serial.println(pClient->getRssi());
    
    /** Now we can read/write/subscribe the charateristics of the services we are interested in */
    NimBLERemoteService* pSvc = nullptr;
    NimBLERemoteCharacteristic* pChr = nullptr;
    NimBLERemoteDescriptor* pDsc = nullptr;
    // the original was setup for service/characteristic : DEAD-BEEF   ..but we are revising for BAAD/F00D
    pSvc = pClient->getService(_BLEClientServiceUUID);  // char* _BLEClientServiceUUID   36 characters
    if (pSvc) 
    {     /** make sure it's not null */
        pChr = pSvc->getCharacteristic(_BLEClientCharacteristicUUID);  //this was the original example code characteristic called  "BEEF"
        _BLEClientCharacteristicFeed = pChr;
        if (!pChr) 
        {
          Serial.println(" **** BLEClientNetworking is nil is creation...");
        }
        else
        {     /** make sure it's not null */
            if (pChr->canRead()) 
            {
                Serial.print(pChr->getUUID().toString().c_str());
                Serial.print(" Value: ");
                Serial.println(pChr->readValue().c_str());
            }
            
            if (pChr->canWrite()) 
            {
                if (pChr->writeValue("Tasty")) {
                    Serial.print("Wrote new value to: ");
                    Serial.println(pChr->getUUID().toString().c_str());
                }
                else 
                {
                  Serial.println("Disconnecting if write failed");
                    /** Disconnect if write failed */
                    pClient->disconnect();
                    return false;
                }

                // read reply..
                if (pChr->canRead()) 
                {
                    Serial.print("The value of: ");
                    Serial.print(pChr->getUUID().toString().c_str());
                    Serial.print(" is now: ");
                    Serial.println(pChr->readValue().c_str());
                }
            }
            
            /** registerForNotify() has been deprecated and replaced with subscribe() / unsubscribe().
             *  Subscribe parameter defaults are: notifications=true, notifyCallback=nullptr, response=false.
             *  Unsubscribe parameter defaults are: response=false.
             */
            if (pChr->canNotify()) 
            {
                //if(!pChr->registerForNotify(notifyCB)) {
                if (!pChr->subscribe(true, notifyCB)) {
                    /** Disconnect if subscribe failed */
                    pClient->disconnect();
                    return false;
                }
            }
            else if (pChr->canIndicate()) 
            {
                /** Send false as first argument to subscribe to indications instead of notifications */
                //if(!pChr->registerForNotify(notifyCB, false)) {
                if (!pChr->subscribe(false, notifyCB)) 
                {
                    /** Disconnect if subscribe failed */
                    pClient->disconnect();
                    return false;
                }
            }
        }
        
    }
    else 
    {
        Serial.println("DEAD service not found.");
    }
    // original example  pSvc = pClient->getService("BAAD"); // reference only---remove after testing
    if (pSvc) 
    {     /** make sure it's not null */
        // origional example:  pChr = pSvc->getCharacteristic("F00D"); // reference only---remove after testing
        if (pChr) 
        {     /** make sure it's not null */
            if (pChr->canRead()) 
            {
                Serial.print(pChr->getUUID().toString().c_str());
                Serial.print(" Value: ");
                Serial.println(pChr->readValue().c_str());
            }
            
            pDsc = pChr->getDescriptor(NimBLEUUID("C01D"));
            if (pDsc) 
            {   /** make sure it's not null */
                Serial.print("Descriptor: ");
                Serial.print(pDsc->getUUID().toString().c_str());
                Serial.print(" Value: ");
                Serial.println(pDsc->readValue().c_str());
            }
            
            if (pChr->canWrite()) 
            {
                if (pChr->writeValue("No tip!")) {
                    Serial.print("Wrote new value to: ");
                    Serial.println(pChr->getUUID().toString().c_str());
                }
                else {
                    /** Disconnect if write failed */
                    pClient->disconnect();
                    return false;
                }
                
                if (pChr->canRead()) {
                    Serial.print("The value of: ");
                    Serial.print(pChr->getUUID().toString().c_str());
                    Serial.print(" is now: ");
                    Serial.println(pChr->readValue().c_str());
                }
            }
            
            /** registerForNotify() has been deprecated and replaced with subscribe() / unsubscribe().
             *  Subscribe parameter defaults are: notifications=true, notifyCallback=nullptr, response=false.
             *  Unsubscribe parameter defaults are: response=false.
             */
            if (pChr->canNotify()) {
                //if(!pChr->registerForNotify(notifyCB)) {
                if (!pChr->subscribe(true, notifyCB)) {
                    /** Disconnect if subscribe failed */
                    pClient->disconnect();
                    return false;
                }
            }
            else if (pChr->canIndicate()) {
                /** Send false as first argument to subscribe to indications instead of notifications */
                //if(!pChr->registerForNotify(notifyCB, false)) {
                if (!pChr->subscribe(false, notifyCB)) {
                    /** Disconnect if subscribe failed */
                    pClient->disconnect();
                    return false;
                }
            }
        }
        
    }
    else {
        Serial.println("BAAD service not found.");
    }
    
    Serial.println("Done with this device!");
    return true;
}

//!sends the "feed" command over bluetooth to the connected device..
void sendFeedCommandBLEClient()
{
  Serial.println("sendFeedCommandBLEClient()");
   // if (FeedFlag == true)
    { //flag is set in  button_task
                            // Set the characteristic's value to be the array of bytes that is actually a string.
        std::string newValue = "s"; // this sets a value to the GATT which is the trigger value for the BLE server feeder
        const uint8_t newValueFeed = { 0x00 };
        if (!_BLEClientCharacteristicFeed)
        {
          Serial.println(" **** Error _BLEClientCharacteristicFeed is nil ***");
          return;
        }
        if (_BLEClientCharacteristicFeed->writeValue(newValueFeed, 1)) {  //force the length to 1.  newValue.length() may return 0 if newValue=null
            Serial.print(newValue.c_str());
            Serial.println("sent FEED");
           // PrevTriggerTime = millis();
           // FeedCount--;
            //         M5.Beep.beep(); //M5.Beep.tone(4000,200);
            // delay(50);
            // M5.Beep.mute();
//            if (FeedCount <= 0){
//                FeedCount = 0;//avoid display of negative values
//            }
//            M5.Lcd.setCursor(0, 80);
//            M5.Lcd.printf("Treats= %d \n\r",FeedCount);
        }
        else {
            Serial.print(newValue.c_str());
            Serial.println("FAILED GATT write");
        }
      //  FeedFlag = false;
        //  delay(100);
        /* check for the acknowledge from the server which is 0x01   */
        if (_BLEClientCharacteristicFeed->canRead()) {
            std::string value = _BLEClientCharacteristicFeed->readValue();
            Serial.print("BLE Server response: ");
            Serial.println(value.c_str());
            if (value[0] == 0x01) {  // server ack is 0x01....so flash client led for the ack from the server  wha 9-28-21
//                Led_ON();
//                delay(150);
//                Led_OFF();//
//                delay(150);
                
                //call what is registered
                callCallbackBLEClient(CALLBACK_BLINK_LIGHT,(char*)"blink");

            }
            else
            {
              Serial.println("Didn't get a response 0x01");
                  //call what is registered
                callCallbackBLEClient(CALLBACK_BLINK_LIGHT,(char*)"blink");
            }
        }
    }
}


//!the setup() and loop() passing the serviceName to look for..
void setupBLEClientNetworking(char *serviceName, char *_BLEClientServiceUUID, char *characteristicUUID)
{
    // store these are the client BLE we are looking for..
    //    __BLEClientServiceUUID = _BLEClientServiceUUID;
    //    __BLEClientCharacteristicUUID = characteristicUUID;
    
    //start the bluetooth discovery..
    
    Serial.println("Starting NimBLE BLEClientNetworking");
    /** Initialize NimBLE, no device name spcified as we are not advertising */
    NimBLEDevice::init("");
    
    
    /** Set the IO capabilities of the device, each option will trigger a different pairing method.
     *  BLE_HS_IO_KEYBOARD_ONLY    - Passkey pairing
     *  BLE_HS_IO_DISPLAY_YESNO   - Numeric comparison pairing
     *  BLE_HS_IO_NO_INPUT_OUTPUT - DEFAULT setting - just works pairing
     */
    //NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY); // use passkey
    //NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO); //use numeric comparison
    
    /** 2 different ways to set security - both calls achieve the same result.
     *  no bonding, no man in the middle protection, secure connections.
     *
     *  These are the default values, only shown here for demonstration.
     */
    //NimBLEDevice::setSecurityAuth(false, false, true);
    NimBLEDevice::setSecurityAuth(/*BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM |*/ BLE_SM_PAIR_AUTHREQ_SC);
    
    /** Optional: set the transmit power, default is 3db */
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); /** +9db */
    
    /** Optional: set any devices you don't want to get advertisments from */
    // NimBLEDevice::addIgnored(NimBLEAddress ("aa:bb:cc:dd:ee:ff"));
    
    /** create new scan */
    NimBLEScan* pScan = NimBLEDevice::getScan();
    
    /** create a callback that gets called when advertisers are found */
    pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
    
    /** Set scan interval (how often) and window (how long) in milliseconds */
    pScan->setInterval(45);
    pScan->setWindow(15);
    
    /** Active scan will gather scan response data from advertisers
     *  but will use more energy from both devices
     */
    pScan->setActiveScan(true);
    /** Start scanning for advertisers for the scan time specified (in seconds) 0 = forever
     *  Optional callback for when scanning stops.
     */
    pScan->start(_scanTime, scanEndedCB_BLEClient);
    //M5.Beep.tone(1000);
    // M5.Beep.setVolume(90);  //???? not sure this call is working wha
}

//!the loop()
void loopBLEClientNetworking()
{
    if (_doConnect == true) {   //_doConnect is a command to try and connect using connectToServer_BLEClient()  wha 8-11-21
        if (connectToServer_BLEClient()) {
            Serial.println("-We are now connected to the BLE Server.");
//            Led_OFF();// indicates scanning stopped  wha
//            M5.Lcd.setCursor(210, 50, 1);
//            M5.Lcd.setTextColor(BLUE, YELLOW);
//            M5.Lcd.printf("C\r\n");// C = connected BLE
//            M5.Lcd.setTextColor(WHITE, BLACK);
        }
        else {
            Serial.println("We have failed to connect to the server; there is nothin more we will do.");
        }
        _doConnect = false;
        //  0=stop scanning after first device found
        NimBLEDevice::getScan()->start(_scanTime, scanEndedCB_BLEClient); //resume scanning for more BLE servers
    }
    
}
