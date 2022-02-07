//
//  BLENetworking.cpp
//  M5Stick
//
//  Created by Scott Moody on 1/19/22.
//

#include "BLEServerNetworking.h"
#include <NimBLEDevice.h>
#include <Arduino.h>
#define pettutorApproach

//!the BLE Server
static NimBLEServer* _pBLEServer;
NimBLECharacteristic* _pCharacteristic;

//!performs the indirect callback based on the callbackType
void callCallbackBLEServer(int callbackType, char *message);

//!need to device way to change these ...
//#define SERVICE_UUID        "B0E6A4BF-CCCC-FFFF-330C-0000000000F0"  //Pet Tutor feeder service for feed
//#define CHARACTERISTIC_UUID "B0E6A4BF-CCCC-FFFF-330C-0000000000F1"  //Pet Tutor feeder characteristic
char *_SERVICE_UUID;
char *_CHARACTERISTIC_UUID;

#define MAX_MESSAGE 500
char _asciiMessage[MAX_MESSAGE];

//!sets the device name in the advertising
void setBLEServerDeviceName(char *deviceName)
{
    _pCharacteristic->setValue(deviceName);
}

/**  None of these are required as they will be handled by the library with defaults. **
 **                       Remove as you see fit for your needs                        */
class BLEServeNetworkingCallbacks : public NimBLEServerCallbacks
{
    void onConnect(NimBLEServer* pServer)
    {
        Serial.println("Client connected");
        Serial.println("Multi-connect support: start advertising");
        NimBLEDevice::startAdvertising();
    };
    /** Alternative onConnect() method to extract details of the connection.
     See: src/ble_gap.h for the details of the ble_gap_conn_desc struct.
     */
    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc)
    {
        Serial.print("Client address: ");
        Serial.println(NimBLEAddress(desc->peer_ota_addr).toString().c_str());
        /** We can use the connection handle here to ask for different connection parameters.
         Args: connection handle, min connection interval, max connection interval
         latency, supervision timeout.
         Units; Min/Max Intervals: 1.25 millisecond increments.
         Latency: number of intervals allowed to skip.
         Timeout: 10 millisecond increments, try for 5x interval time for best results.
         */
        //GOOD:
        pServer->updateConnParams(desc->conn_handle, 24, 48, 0, 60);
        
        //try:
        //pServer->updateConnParams(desc->conn_handle, 100, 200, 10, 160);
        
    };
    void onDisconnect(NimBLEServer* pServer)
    {
        Serial.println("Client disconnected - start advertising");
        NimBLEDevice::startAdvertising();
    };
    void onMTUChange(uint16_t MTU, ble_gap_conn_desc* desc)
    {
        Serial.printf("MTU updated: %u for connection ID: %u\n", MTU, desc->conn_handle);
    };
    
    /********************* Security handled here **********************
     ****** Note: these are the same return values as defaults ********/
    uint32_t onPassKeyRequest()
    {
        Serial.println("Server Passkey Request");
        /** This should return a random 6 digit number for security
         or make your own static passkey as done here.
         */
        return 123456;
    };
    
    bool onConfirmPIN(uint32_t pass_key)
    {
        Serial.print("The passkey YES/NO number: "); Serial.println(pass_key);
        /** Return false if passkeys don't match. */
        return true;
    };
    
    void onAuthenticationComplete(ble_gap_conn_desc* desc)
    {
        /** Check that encryption was successful, if not we disconnect the client */
        if (!desc->sec_state.encrypted)
        {
            NimBLEDevice::getServer()->disconnect(desc->conn_handle);
            Serial.println("Encrypt connection failed - disconnecting client");
            return;
        }
        Serial.println("Starting BLE work!");
    };
};

/** Handler class for characteristic actions */
//!Note: the onRead and onWrite are from the perspective of the client. So onRead is the client calling this server, over BLE, and reading a value. The onWrite is the client writing to this server (sending a BLE message). This is where JSON or raw values are used.
class BLEServerNetworkingCharacteristicCallbacks : public NimBLECharacteristicCallbacks
{
    //!called when wanting to "read" from this server
    void onRead(NimBLECharacteristic* pCharacteristic)
    {
        Serial.print(pCharacteristic->getUUID().toString().c_str());
        Serial.print(": onRead(), value: ");
        // Serial.print(_fullJSONString);
        Serial.println(pCharacteristic->getValue().c_str());
    };
    
    //!called when writing to the server.
    void onWrite(NimBLECharacteristic* pCharacteristic)
    {
        Serial.print(pCharacteristic->getUUID().toString().c_str());
        Serial.print(": onWrite(), value: ");
        Serial.println(pCharacteristic->getValue().c_str());
        std::string rxValue = pCharacteristic->getValue().c_str();
        //??
        //      ProcessClientCmd(rxValue[0]);   //?? client sent a command now see what it is
        //      pCharacteristic->setValue(0x01);  //??  This is the acknowlege(ACK) back to client.  Later this should be contigent on a feed completed
        /*******************************MQTT*************************************/
        std::string value = pCharacteristic->getValue();
        
        if (value.length() > 0) {
            Serial.println("*********");
            Serial.printf("New value: %d - ", value.length());
            
            //create a string 'ascii' from the values
            for (int i = 0; i < value.length(); i++)
            {
                _asciiMessage[i] = value[i];
                _asciiMessage[i + 1] = 0;
                if (i >= value.length())
                    break;
            }
            
            
            
            //call the callback specified from the caller (eg. NimBLE_PetTutor_Server .. or others)
            // (*_callbackFunction)(rxValue);
            callCallbackBLEServer(CALLBACK_ONWRITE, _asciiMessage);
            
#ifdef MQTT_NETWORKING
            //This should be a 'register' command..
            
            // will return true if the message was JSON and was processes, false otherwise.
            if (processJSONMessage(ascii))
            {
                //processed by the MQTTNetworking code..
                pCharacteristic->setValue(0x01);  //??  This is the acknowlege(ACK) back to client.  Later this should be contigent on a feed completed
            }
            else
            {
                //perform feed...
                Serial.println("Perform FEED");
                
                //std::string rxValue = pCharacteristic->getValue().c_str();  //??
                ProcessClientCmd(rxValue[0]);   //?? client sent a command now see what it is
                pCharacteristic->setValue(0x01);  //??  This is the acknowlege(ACK) back to client.  Later this should be contigent on a feed completed
                
            }
            Serial.println("*********");
            
#endif
        }
        
        /*******************************MQTT*************************************/
        
    };
    
    /** Called before notification or indication is sent,
     the value can be changed here before sending if desired.
     */
    void onNotify(NimBLECharacteristic * pCharacteristic)
    {
        Serial.println("Sending notification to clients");
    };
    
    
    /** The status returned in status is defined in NimBLECharacteristic.h.
     The value returned in code is the NimBLE host return code.
     */
    void onStatus(NimBLECharacteristic * pCharacteristic, Status status, int code)
    {
        String str = ("Notification/Indication status code: ");
        str += status;
        str += ", return code: ";
        str += code;
        str += ", ";
        str += NimBLEUtils::returnCodeToString(code);
        Serial.println(str);
    };
    
    void onSubscribe(NimBLECharacteristic * pCharacteristic, ble_gap_conn_desc * desc, uint16_t subValue)
    {
        String str = "Client ID: ";
        str += desc->conn_handle;
        str += " Address: ";
        str += std::string(NimBLEAddress(desc->peer_ota_addr)).c_str();
        if (subValue == 0) {
            str += " Unsubscribed to ";
        }
        else if (subValue == 1) {
            str += " Subscribed to notfications for ";
        }
        else if (subValue == 2) {
            str += " Subscribed to indications for ";
        }
        else if (subValue == 3) {
            str += " Subscribed to notifications and indications for ";
        }
        str += std::string(pCharacteristic->getUUID()).c_str();
        
        Serial.println(str);
    };
};

/** Handler class for descriptor actions */
class BLEServerNetworkingDescriptorCallbacks : public NimBLEDescriptorCallbacks
{
    void onWrite(NimBLEDescriptor* pDescriptor)
    {
        std::string dscVal((char*)pDescriptor->getValue(), pDescriptor->getLength());
        Serial.print("Descriptor written value:");
        Serial.println(dscVal.c_str());
    };
    
    void onRead(NimBLEDescriptor* pDescriptor)
    {
        Serial.print(pDescriptor->getUUID().toString().c_str());
        Serial.println("Descriptor read");
    };
};


/** Define callback instances globally to use for multiple Charateristics \ Descriptors */
//static DescriptorCallbacks dscCallbacks;
//static CharacteristicCallbacks chrCallbacks;


/** Define callback instances globally to use for multiple Charateristics \ Descriptors */
static BLEServerNetworkingDescriptorCallbacks _descriptorBLEServerCallbacks;
static BLEServerNetworkingCharacteristicCallbacks _characteristicBLEServerCallbacks;

//!the 'loop' for this module BLEServerNetworking. 
void loopBLEServerNetworking()
{
#ifdef NOBLINK
    if (_pBLEServer->getConnectedCount() == 0) {  //?? flash led if no clients connected. note: advertising continues with clients connected  wha 8-26-21
        BlinkLed();
    }
    else {
        digitalWrite(LED, LOW);//?? turn off led if 1 or more clients connected (up to 3 clients is the default but can be configured to 9 clients) wha 8-26-21
    }
#endif
    //    Serial.printf("# Clients:  %d \n\r", pServer->getConnectedCount()); // nimconfig.h can change the max clients allowed(up to 9). default is 3  wha
#ifdef originalApproach
    if (_pBLEServer->getConnectedCount()) {
        NimBLEService* pSvc = pServer->getServiceByUUID("BAAD");
        if (pSvc) {
            NimBLECharacteristic* pChr = pSvc->getCharacteristic("F00D");
            if (pChr) {
                pChr->notify(true);
            }
        }
    }
#endif
#ifdef pettutorApproach
    if (_pBLEServer->getConnectedCount()) {
        NimBLEService* pSvc = _pBLEServer->getServiceByUUID(_SERVICE_UUID);
        if (pSvc) {
            NimBLECharacteristic* pChr = pSvc->getCharacteristic(_CHARACTERISTIC_UUID);
            if (pChr) {
                pChr->notify(true);
            }
        }
    }
#endif
}
//!the 'setup' for this module BLEServerNetworking. Here the service name is added (and potentially more later)
void setupBLEServerNetworking(char *serviceName, char * deviceName, char *serviceUUID, char *characteristicUUID)
{
    _SERVICE_UUID = serviceUUID;
    _CHARACTERISTIC_UUID = characteristicUUID;

    Serial.printf("setupBLEServerNetworking(%s,%s,%s,%s)\n", serviceName, deviceName, serviceUUID, characteristicUUID);
    
    /** sets device name */
    //??original    NimBLEDevice::init("NimBLE-Arduino");
    //  NimBLEDevice::init("PTFeeder");
    NimBLEDevice::init(serviceName);
    
    /** Optional: set the transmit power, default is 3db */
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); /** +9db */
    
    /** Set the IO capabilities of the device, each option will trigger a different pairing method.
     BLE_HS_IO_DISPLAY_ONLY    - Passkey pairing
     BLE_HS_IO_DISPLAY_YESNO   - Numeric comparison pairing
     BLE_HS_IO_NO_INPUT_OUTPUT - DEFAULT setting - just works pairing
     */
    //NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY); // use passkey
    //NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO); //use numeric comparison
    
    /** 2 different ways to set security - both calls achieve the same result.
     no bonding, no man in the middle protection, secure connections.
     
     These are the default values, only shown here for demonstration.
     */
    //NimBLEDevice::setSecurityAuth(false, false, true);
    NimBLEDevice::setSecurityAuth(/*BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM |*/ BLE_SM_PAIR_AUTHREQ_SC);
    
    _pBLEServer = NimBLEDevice::createServer();
    _pBLEServer->setCallbacks(new BLEServerCallbacks());
    /******************  DEAD service              BEEF characteristic 2904notify,R/W/W_ENC(pairing REQUIRED!)                    ********************/
    NimBLEService* pDeadService = _pBLEServer->createService("DEAD");
    //??   NimBLEService* pDeadService = pServer->createService(SERVICE_UUID);
    NimBLECharacteristic* pBeefCharacteristic = pDeadService->createCharacteristic(
                                                                                   "BEEF",
                                                                                   //?? NimBLECharacteristic * pBeefCharacteristic = pDeadService->createCharacteristic(
                                                                                   //??    CHARACTERISTIC_UUID,
                                                                                   NIMBLE_PROPERTY::READ |
                                                                                   NIMBLE_PROPERTY::WRITE |
                                                                                   /** Require a secure connection for read and write access */
                                                                                   NIMBLE_PROPERTY::READ_ENC |  // only allow reading if paired / encrypted
                                                                                   NIMBLE_PROPERTY::WRITE_ENC   // only allow writing if paired / encrypted
                                                                                   );
    
    //assign to global:
    _pCharacteristic = pBeefCharacteristic;
    
    /*******************************MQTT*************************************/
    //#ifdef MQTT_NETWORKING
    pBeefCharacteristic->setValue(deviceName);
    
    //#else
    //    pBeefCharacteristic->setValue("Burger");
    //#endif
    /*******************************MQTT*************************************/
    
    pBeefCharacteristic->setCallbacks(&_characteristicBLEServerCallbacks);
    
    /** 2904 descriptors are a special case, when createDescriptor is called with
     0x2904 a NimBLE2904 class is created with the correct properties and sizes.
     However we must cast the returned reference to the correct type as the method
     only returns a pointer to the base NimBLEDescriptor class.
     */
    NimBLE2904* pBeef2904 = (NimBLE2904*)pBeefCharacteristic->createDescriptor("2904");
    pBeef2904->setFormat(NimBLE2904::FORMAT_UTF8);
    pBeef2904->setCallbacks(&_descriptorBLEServerCallbacks);
    
    /********  BAAD service              F00D characteristic R/W/N  C01D Descriptor                   ********************************/
#ifdef originalApproach
    NimBLEService* pBaadService = pServer->createService("BAAD");
    NimBLECharacteristic* pFoodCharacteristic = pBaadService->createCharacteristic(
                                                                                   "F00D",
                                                                                   NIMBLE_PROPERTY::READ |
                                                                                   NIMBLE_PROPERTY::WRITE |
                                                                                   NIMBLE_PROPERTY::NOTIFY
                                                                                   );
#endif
#ifdef pettutorApproach
    NimBLEService* pBaadService = _pBLEServer->createService(_SERVICE_UUID);
    NimBLECharacteristic* pFoodCharacteristic = pBaadService->createCharacteristic(
                                                                                   _CHARACTERISTIC_UUID,
                                                                                   NIMBLE_PROPERTY::READ |
                                                                                   NIMBLE_PROPERTY::WRITE // |
                                                                                   //       NIMBLE_PROPERTY::NOTIFY
                                                                                   );
#endif
    
#ifdef originalApproach
    pFoodCharacteristic->setValue("Fries");
#endif
    
    pFoodCharacteristic->setCallbacks(&_characteristicBLEServerCallbacks);
    
    /** Note a 0x2902 descriptor MUST NOT be created as NimBLE will create one automatically
     if notification or indication properties are assigned to a characteristic.
     */
    
#ifdef originalApproach
    /** Custom descriptor: Arguments are UUID, Properties, max length in bytes of the value */
    NimBLEDescriptor* pC01Ddsc = pFoodCharacteristic->createDescriptor(
                                                                       "C01D",
                                                                       NIMBLE_PROPERTY::READ |
                                                                       NIMBLE_PROPERTY::WRITE |
                                                                       NIMBLE_PROPERTY::WRITE_ENC, // only allow writing if paired / encrypted
                                                                       20
                                                                       );
    pC01Ddsc->setValue("Send it back!");
    pC01Ddsc->setCallbacks(&dscCallbacks);
#endif
#ifdef pettutorApproach
    /** Custom descriptor: Arguments are UUID, Properties, max length in bytes of the value */
    NimBLEDescriptor* pPetTutordsc = pFoodCharacteristic->createDescriptor(
                                                                           "C01D", //the UUID is 0xC01D
                                                                           NIMBLE_PROPERTY::READ |
                                                                           NIMBLE_PROPERTY::WRITE |
                                                                           20
                                                                           );
    pPetTutordsc->setValue("feed s/a/j type u/m ");
    pPetTutordsc->setCallbacks(&_descriptorBLEServerCallbacks);
#endif
    
    // end setup of services and characteristics
    
    /** Start the services when finished creating all Characteristics and Descriptors */
    pDeadService->start();
    pBaadService->start();
    
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    /** Add the services to the advertisment data **/
    pAdvertising->addServiceUUID(pDeadService->getUUID());
    pAdvertising->addServiceUUID(pBaadService->getUUID());
    /** If your device is battery powered you may consider setting scan response
     to false as it will extend battery life at the expense of less data sent.
     */
    pAdvertising->setScanResponse(true);
    pAdvertising->start();
    
    Serial.println("Advertising Started");
}


//!send ACK over bluetooth, this right now is 0x01
void sendBLEMessageACKMessage()
{
    _pCharacteristic->setValue(0x01);  //??  This is the acknowlege(ACK) back to client.  Later this should be contigent on a feed completed
}

//************* BLEServer Callback Registrations **************************/

//!example callback: but the scope would have the pCharacteristic defined, etc..
void dummyBLEServerCallback(char *message)
{
    Serial.println("*** No callback defined ..");
}


//#define CALLBACK_FEED 0
//#define CALLBACK_BLINK 1
//#define CALLBACK_TEMP 2
//!define as: void callback(char* message)
//!  call processMessage(message, &callback);
//#define MAX_CALLBACKS 2
//!the array of callback functions
void (*_callbackFunctionsBLEServer[MAX_CALLBACKS])(char *);
//!flag for initializing if not yes
int _callbacksInitializedBLEServer = false;
//!init the callbacks to dummy callbacks
void initCallbacksBLEServer()
{
    for (int i = 0; i < MAX_CALLBACKS; i++)
    {
        _callbackFunctionsBLEServer[i] = &dummyBLEServerCallback;
    }
}

//!register the callback based on the callbackType
void registerCallbackBLEServer(int callbackType, void (*callback)(char*))
{
    if (!_callbacksInitializedBLEServer)
    {
        _callbacksInitializedBLEServer = true;
        initCallbacksBLEServer();
    }
    if (callbackType < 0 || callbackType >= MAX_CALLBACKS)
    {
        Serial.println("#### Error outside callback range");
    }
    else
    {
        _callbackFunctionsBLEServer[callbackType] = callback;
        Serial.printf("registerCallbackBLEServer(%d)\n", callbackType);
    }
}
//!performs the indirect callback based on the callbackType
void callCallbackBLEServer(int callbackType, char *message)
{
    if (callbackType < 0 || callbackType >= MAX_CALLBACKS)
    {
        Serial.println("#### Error outside callback range");
    }
    else
    {
        void (*callbackFunctionsBLEServer)(char *) = _callbackFunctionsBLEServer[callbackType];
        (*callbackFunctionsBLEServer)(message);
    }
}
