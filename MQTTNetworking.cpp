/** The MQTT + WIFI part
 
 
 Created: on Jan 1, 2022
 Author: Scott Moody
 test..
 */

#define VERSION "Version 1.2 01.19.2022"
#include "MQTTNetworking.h"
#include <Arduino.h>

/*******************************MQTT*************************************/
//!added 1.1.2022 by iDogWatch.com

//https://www.arduino.cc/en/Reference/BLECharacteristicConstructor
#include <Arduino_JSON.h>
#include <Preferences.h>

#include <WiFi.h>  //look for a lighter weight version...
#include <PubSubClient.h>
//NOTE: NOT NOW.. had to turn off the OTA to reduce the size of the executable... CHECK with MQTT??

// wifi config store.  wifi配置存储的类
Preferences _preferences;

//forward declarations
//!process an MQTT message looking for keywords (barklet language)
void processBarkletMessage(String message);

//!setup the MQTT server
void setupMQTT(char* mqttServerString, char *mqttPortString, char *mqttPasswordString, char *mqttUserString, char *deviceNameString, char *uuidString); //forward

//check for MQTT messages???
void checkMQTTMessages();

//!get the chip info
void getChipInfo();

//!setup the WIFI
void setupWIFI(char *ssidString, char *ssidPasswordString);
//! if an empty string
boolean isEmptyString(char *stringA);
//!check if the string contains the other string
bool containsSubstring(String message, String substring);
//!read any values from EPROM
void readPreferences();

//!define here as well.. NOTE: this could be passed is as well... TODO

// flag that the MQTT is running..
boolean _MQTTRunning = false;

//!define this storage once, and use everwhere..
#define MAX_MESSAGE 300
char _fullMessage[MAX_MESSAGE];

//!Points to strings read from JSON (limited to 15 char key name)
#define _preferencesJSONName "JSONPrefs"
char* _ssidString;
char* _ssidPasswordString;
char* _mqttServerString;
char* _mqttPortString;
char* _mqttPasswordString;
char* _mqttUserString;
char* _mqttTopicString;
char* _deviceNameString;
char* _uuidString;
char* _jsonVersionString;
char* _jsonHeaderString;

String _fullJSONString;

uint32_t _chipId = 0;
char _chipName[50];

//!example callback: but the scope would have the pCharacteristic defined, etc..
void dummyCallback(char *message)
{
    Serial.println("No callback defined ..");
}


//#define CALLBACK_FEED 0
//#define CALLBACK_BLINK 1
//#define CALLBACK_TEMP 2
//!define as: void callback(char* message)
//!  call processMessage(message, &callback);
#define MAX_CALLBACKS 3
//!the array of callback functions
void (*callbackFunctions[MAX_CALLBACKS])(char *);
//!flag for initializing if not yes
int _callbacksInitialized = false;
//!init the callbacks to dummy callbacks
void initCallbacks()
{
    for (int i = 0; i < MAX_CALLBACKS; i++)
    {
        callbackFunctions[i] = &dummyCallback;
    }
}

//!register the callback based on the callbackType
void registerCallback(int callbackType, void (*callback)(char*))
{
    if (!_callbacksInitialized)
    {
        _callbacksInitialized = true;
        initCallbacks();
    }
    if (callbackType < 0 || callbackType >= MAX_CALLBACKS)
    {
        Serial.println("#### Error outside callback range");
    }
    else
    {
        callbackFunctions[callbackType] = callback;
    }
}
//!performs the indirect callback based on the callbackType
void callCallback(int callbackType, char *message)
{
    if (callbackType < 0 || callbackType >= MAX_CALLBACKS)
    {
        Serial.println("#### Error outside callback range");
    }
    else {
        void (*callbackFunction)(char *) = callbackFunctions[callbackType];
        (*callbackFunction)(message);
    }
}

//! retrieve the Configuration JSON string in JSON format..
String getJSONConfigString()
{
    return _fullJSONString;
}

//!callback with the message if required (like sending the FEED message)
//!!function pointers: https://www.cprogramming.com/tutorial/function-pointers.html
//!define as: void callback(char* message)
//!  call processMessage(message, &callback);
//void setMessageCallbacks(void (*callbackFunction)(char*), void (*blinkTheLED)())

//!called for things like the advertisement
char *getDeviceName()
{
    return _chipName;
}

//!setup the MQTT part of networking
void setupMQTTNetworking()
{
    // sets the _chipName
    getChipInfo();
    //THIS should output the device name, user name, etc...
    Serial.println(_chipName);
    
    //read what was stored, if any..
    readPreferences();
    if (_ssidString)
    {
        //setup with WIFI
        setupWIFI(_ssidString, _ssidPasswordString);
        
        // now setup the MQTT
        setupMQTT(_mqttServerString, _mqttPortString, _mqttPasswordString, _mqttUserString, _deviceNameString, _uuidString);
        
    }
}

//! called for the loop() of this plugin
void loopMQTTNetworking()
{
    if (_MQTTRunning)
    {
        checkMQTTMessages();
    }
}

// ******************************MQTT + WIFI ***********************************


// ******************************HELPER METHODS, ***********************************


//https://www.arduino.cc/en/Tutorial/Foundations/DigitalPins
void blinkBlueLightMQTT()
{
    //call method passed in..
    //  if (_blinkTheLED)
    //      (*_blinkTheLED)();
    callCallback(CALLBACK_BLINK, strdup("blink"));
}

//!create a unique ID (but it needs to be stored.. otherwise it's unique each time??
void getChipInfo()
{
    
    //get chip ID
    for (int i = 0; i < 17; i = i + 8) {
        _chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }
    
    if (_deviceNameString)
        sprintf(_chipName, "%s-%d", _deviceNameString, _chipId);
    else
        sprintf(_chipName, "esp.%d", _chipId);
    
    //chipName = "esp." + chipId;
    Serial.println(_chipName);
    
}


//Methods:  setupMQTT();
//          checkMQTTMessages()


#define STATUS "#STATUS"
//NOTE: the "Me" names are known and keep them..
#define REMOTE "#remoteMe"
#define FEED "#FEED"
#define FEED_2 "#feedme"
#define ACK_FEED "#actMe"
#define CONNECTED "#connectedMe"
#define NOT_CONNECTED "#noconnectedMe"
#define NO_ACK_FEED "#noactMe"
#define PLAY_ME "#playMe"
#define DOCFOLLOW "#docFollow"
#define DOCSYNC "#docSync"
#define NO_CAN "#NO_CAN"

//!The WIFI client
WiFiClient _espClient;

//!The PubSub MQTT Client
PubSubClient _mqttClient(_espClient);

//!setup the WIFI using ssid and password
void setupWIFI(char * arg_ssid, char * arg_password)
{
    delay(10);
    // We start by connecting to a WiFi network
    Serial.printf("\nConnecting to '%s' password = '%s'", arg_ssid, arg_password);
    
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(arg_ssid, arg_password);
    int count = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
        count++;
        if (count > 10)
        {
            count = 0;
            Serial.println();
        }
    }
    Serial.println();
    
    //random ??  for the WIFI address?
    randomSeed(micros());
    
    //This creates a DHCP address
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    
    //blink the LED
    blinkBlueLightMQTT();
    
}

//!called when data on the MQTT socket
void callbackMQTTHandler(char* topic, byte* payload, unsigned int length)
{
    int i = 0;
    for (i = 0; i < length && i < MAX_MESSAGE; i++) {
        _fullMessage[i] = (char)payload[i];
    }
    _fullMessage[i] = (char)NULL;
    
    //note there is a strange issue where a "null" is showing up in my MQTT/Websocket path
    //eg: Message arrived [idogwatch/bark] Guest35: null
    //sow for now.. not processing if the message has a "null"
    if (containsSubstring(_fullMessage, "null"))
    {
        //don't process..
        return;
    }
    
    Serial.printf("MessageArrived: topic=%s, '%s'\n", topic, _fullMessage);
    
    //process this message (We don't recognize just a JSON config yet..) That arrives on bluetooth
    processBarkletMessage(_fullMessage);
}

//!reconnects and re-subscribes
//!NOTE: we need the host info...
void reconnectMQTT()
{
    // char message[60];
    int count = 0;
    // Loop until we're reconnected
    while (!_mqttClient.connected()) // && count < 10)
    {
        Serial.printf("Attempting MQTT connection: '%s' count=%d\n", _mqttServerString, count);
        
        count++;
        // Create a random client ID
        String clientId = "esp32";
        clientId += String(random(0xffff), HEX);
        
        // Attempt to connect
        //TODO: use the argments...
        if (_mqttClient.connect(clientId.c_str(), _mqttUserString, _mqttPasswordString))
        {
            Serial.println("... CONNECTED");
            blinkBlueLightMQTT();
            
            // Once connected, publish an announcement...
            // sprintf(message, "#STATUS {%s} {%s}", _deviceNameString, chipName);
            sprintf(_fullMessage, "%s {%s}{ssid=%s, mqttServer=%s, mqttUser=%s, topic=%s}", REMOTE, _deviceNameString, _ssidString, _mqttServerString, _mqttUserString, _mqttTopicString);
            
            //publich back on topic
            _mqttClient.publish(_mqttTopicString, _fullMessage);
            
            // ... and resubscribe to same topic
            _mqttClient.subscribe(_mqttTopicString);
            _mqttClient.subscribe(strdup("userP/bark")); //for superuser
            
            //add another topic ... TODO
            _MQTTRunning = true;
            
        }
        else
        {
            Serial.printf("FAILED, rc=%d, trying again in 2 seconds\n", _mqttClient.state());
            
            // Wait 2 seconds before retrying
            delay(2000);
        }
    } //while not connected
}

//!setup the MQTT (called after the WIFI connected)
void setupMQTT(char* mqttServerString, char *mqttPortString, char *mqttPasswordString, char *mqttUserString, char *deviceNameString, char *uuidString)
{
    _MQTTRunning = true;
    
    //connect to server:port
    int port = atoi(mqttPortString);
    _mqttClient.setServer(mqttServerString, port);
    _mqttClient.setCallback(callbackMQTTHandler);
}

//!check for MQTT messages???
void checkMQTTMessages()  //who calls this???
{
    if (!_mqttClient.connected())
    {
        reconnectMQTT();
    }
    _mqttClient.loop(); //WHAT KIND OF LOOP??
}

//!check if the string contains the other string
bool containsSubstring(String message, String substring)
{
    int i;
    bool found = 0;
    //find #
    for (i = 0; i < message.length(); i++)
    {
        if (message[i] == substring[0])
        {
            found = 1;
            break;
        }
    }
    if (found)
    {
        //i == index to start looking
        found = message.substring(i, i + substring.length()) == substring;
    }
    return found;
}
//!! should be a definition that the bluetooth is ONLINE
boolean bluetoothOnline()
{
    return true;
}

//!process an MQTT message looking for keywords (this version uses the Barklet Language Grammer @c 2014)
void processBarkletMessage(String message)
{
    //https://stackoverflow.com/questions/7352099/stdstring-to-char
    
    char *messageString = &message[0];
    
    int valid = 0;
    //    Serial.print("Process MQTT Message: ");
    //    Serial.println(message);
    
    //note: these messages are sent to MQTT. But the messages comming down originated on WebSocket barklets language
    // so the 'remoteMe ..." gets up there, but not back to the rest. It's rewritten by nodered.
    if (containsSubstring(message, STATUS))
    {
        //sprintf(_fullMessage, "%s {%s} {%s} {I,F} {T=now}", REMOTE, _deviceNameString, bluetoothOnline() ? CONNECTED : NOT_CONNECTED);
        // Once connected, publish an announcement...
        // sprintf(message, "#STATUS {%s} {%s}", _deviceNameString, chipName);
        sprintf(_fullMessage, "%s {%s} {%s} {I,F} {T=now} {%s,%s,%s,%s}", REMOTE, _deviceNameString, bluetoothOnline() ? CONNECTED : NOT_CONNECTED, _ssidString, _mqttServerString, _mqttUserString, _mqttTopicString);
        
        
        valid = 1;
    }
    // else if (containsSubstring(message, "#FEED") || containsSubstring(message, "feedme"))
    //only use #FEED (the feedme will turn into #FEED)
    else if (containsSubstring(message, "#FEED") )
    {
        //perform feed...
        Serial.println("Perform FEED internally, calling callbackFunction..");
        
        //char * rxValue = "c";  //feed
        
        //call the callback specified from the caller (eg. NimBLE_PetTutor_Server .. or others)
        // (*_callbackFunction)(rxValue);
        callCallback(CALLBACK_FEED, messageString);
        
        
#ifdef INTERNAL_VERSION
        //internal version means we are sending a bluetooth message to the feeder (we are a client in that mode)
        if (bluetoothOnline())
        {
            Serial.println("sending feed over bluetooth...");
            
            //send over bluetooth... assumes we are connected..
            //###   sendFeedCommand();
            
            //send ack:
            //sprintf(fullMessage, "%s {%s} {FEED} PetTutorFeeder {T=now}", ACK_FEED, _deviceNameString);
            sprintf(_fullMessage, "%s {%s} {T=now}", ACK_FEED, _deviceNameString);
        }
        else
        {
            Serial.println("NOT connected so not sending feed over bluetooth...");
            sprintf(_fullMessage, "%s {%s} {T=now}", NO_ACK_FEED, _deviceNameString);
        }
#else
        sprintf(_fullMessage, "%s {%s} {T=now}", ACK_FEED, _deviceNameString);
#endif
        valid = 1;
        
    }
    else if (containsSubstring(message, "#CAPTURE"))
    {
        sprintf(_fullMessage, "#NO_CAN_CAMERA_CAPTURE {%s} {I am just a chip without a camera}", _deviceNameString);
        valid = 1;
    }
    else if (containsSubstring(message, "#TEMP"))
    {
        sprintf(_fullMessage, "#NO_CAN_GET_TEMP {%s} {I am just a chip without a temp sensor}", _deviceNameString);
        //call the callback specified from the caller (eg. NimBLE_PetTutor_Server .. or others)
        // (*_callbackFunction)(rxValue);
        callCallback(CALLBACK_TEMP, messageString);
        valid = 1;
    }
    if (valid)
    {
        //publish this message..
        _mqttClient.publish(_mqttTopicString, _fullMessage);
        Serial.printf("Sending message: %s\n", _fullMessage);
        //blink the light
        blinkBlueLightMQTT();
    }
    
}


//!If nil it create one with just the null, so strlen = 0
//!NOTE: the strdup() might be used later..
char* createCopy(char * stringA)
{
    if (stringA)
        return strdup(stringA);
    else
        return strdup("");
}

//!informs if null or empty string
boolean isEmptyString(char *stringA)
{
    if (!stringA)
    {
        return true;
    }
    else if (strlen(stringA) == 0)
    {
        return true;
    }
    else
        return false;
}

//This is in case corruption when changing what's written.. defining BOOTSTRAP will clean up the EPROM
//#define BOOTSTRAP

//!read the eprom..
void readPreferences()
{
    
#define BOOTSTRAP
#ifdef BOOTSTRAP
    //note: this could be a 'rebootstrap' message via MQTT .. in the future..
    {
        
        
        Serial.println("BOOTSTRAP device with our own WIFI and MQTT");
        
        char* BOOT_mqtt_server = "iDogWatch.com";
        char* BOOT_mqtt_port = "1883";
        char* BOOT_ssid = "Smart1";   
        char* BOOT_ssid_password = "PET4$tuto5";
        char *BOOT_mqtt_user = "bill";
        char *BOOT_mqtt_password = "pettutor";
        char *BOOT_mqtt_topic = "usersP/bark/bill";
        char *BOOT_deviceName = "TestUno2";
        
        char *BOOT_uuidString = "unused";
        char *BOOT_jsonHeaderString = "WIFI+MQTT";
        char *BOOT_jsonVersionString ="BOOTSTRAP 1.1";

        ///note: these createCopy are to get between String and char* .. probably a better way like  &BOOT[0] or something..
        _ssidString = createCopy(BOOT_ssid);
        _ssidPasswordString = createCopy(BOOT_ssid_password);
        _mqttServerString = createCopy(BOOT_mqtt_server);
        _mqttPortString = createCopy(BOOT_mqtt_port);
        _mqttPasswordString = createCopy(BOOT_mqtt_password);
        _mqttUserString = createCopy(BOOT_mqtt_user);
        _mqttTopicString = createCopy(BOOT_mqtt_topic);
        _deviceNameString = createCopy(BOOT_deviceName);
        _uuidString = createCopy(BOOT_uuidString);
        _jsonHeaderString = createCopy(BOOT_jsonHeaderString);
        _jsonVersionString = createCopy(BOOT_jsonVersionString);
        
        
        //JSON object
        JSONVar myObject;
        
        myObject["ssid"] = BOOT_ssid;
        myObject["ssidPassword"] = BOOT_ssid_password;
        myObject["mqtt_server"] = BOOT_mqtt_server;
        myObject["mqtt_port"] = BOOT_mqtt_port;
        myObject["mqtt_password"] = BOOT_mqtt_password;
        myObject["mqtt_user"] = BOOT_mqtt_user;
        myObject["mqtt_topic"] = BOOT_mqtt_topic;
        myObject["deviceName"] = BOOT_deviceName;
        myObject["uuid"] = BOOT_uuidString;
        myObject["jsonHeader"] = BOOT_jsonHeaderString;
        myObject["jsonVersion"] = BOOT_jsonVersionString;
        //open the preferences
        _preferences.begin("ESP32", false);  //readwrite..
        _preferences.clear();
        //output our object.. myObject has a string version..
        Serial.print("Writing EPROM JSON = ");
        
        Serial.println(myObject);
        _preferences.putString(_preferencesJSONName, myObject);
        
        // Close the Preferences
        _preferences.end();
    }
    return;
#endif //BOOTSTRAP
    
    //https://randomnerdtutorials.com/esp32-save-data-permanently-preferences/
    //https://github.com/espressif/arduino-esp32/blob/master/libraries/Preferences/src/Preferences.cpp
    // Open Preferences with my-app namespace. Each application module, library, etc
    // has to use a namespace name to prevent key name collisions. We will open storage in
    // RW-mode (second parameter has to be false).
    // Note: Namespace name is limited to 15 chars.
    _preferences.begin("ESP32", false);  //false=read/write..
    _fullJSONString = _preferences.getString(_preferencesJSONName);
    Serial.print("Reading EPROM JSON = ");
    Serial.println(_fullJSONString);
    //check ... _fullMessage
    // Close the Preferences
    _preferences.end();
    JSONVar myObject = JSON.parse(_fullJSONString);
    Serial.print("JSON parsed = ");
    Serial.println(myObject);
    
    //defaults:
    _deviceNameString = strdup("Unnamed");
    
    //parse
    const char* a1 = myObject["ssid"];
    _ssidString = const_cast<char*>(a1);
    _ssidString = createCopy(_ssidString);
    
    Serial.println(_ssidString);
    if (!_ssidString)
    {
        
        Serial.print("No _preferences named: ");
        Serial.println(_preferencesJSONName);
        _ssidString = null;
        _ssidPasswordString = null;
        _mqttServerString = null;
        _mqttPortString = null;
        _mqttPasswordString = null;
        _mqttUserString = null;
        _mqttTopicString = null;
        _deviceNameString = strdup("Unnamed");
        _uuidString = null;
        _jsonHeaderString = null;
        _jsonVersionString = null;
        
        return;
        
    }
    //seems the JSON object only returns these const char*, and not easy to just create a char *, so they are created in their own memory..
    const char* a2 = myObject["ssidPassword"];
    if (a2)
    {
        _ssidPasswordString = const_cast<char*>(a2);
        _ssidPasswordString = createCopy(_ssidPasswordString);
        Serial.println(_ssidPasswordString);
    }
    
    //the MQTT host/port/user/password  (topic is created in this code...)
    const char* a3 = myObject["mqtt_server"];
    if (a3)
    {
        _mqttServerString = const_cast<char*>(a3);
        _mqttServerString = createCopy(_mqttServerString);
        Serial.println(_mqttServerString);
    }
    
    
    const char* a4 = myObject["mqtt_port"];
    if (a4)
    {
        _mqttPortString = const_cast<char*>(a4);
        _mqttPortString = createCopy(_mqttPortString);
    }
    
    
    const char* a5 = myObject["mqtt_password"];
    if (a5)
    {
        _mqttPasswordString = const_cast<char*>(a5);
        _mqttPasswordString = createCopy(_mqttPasswordString);
        Serial.println(_mqttPasswordString);
    }
    
    
    const char* a6 = myObject["mqtt_user"];
    if (a6)
    {
        _mqttUserString = const_cast<char*>(a6);
        _mqttUserString = createCopy(_mqttUserString);
    }
    
    
    const char* a7 = myObject["deviceName"];
    if (a7)
    {
        _deviceNameString = const_cast<char*>(a7);
        _deviceNameString = createCopy(_deviceNameString);
    }
    
    //update the chip name with the deviceName
    getChipInfo();
    
    Serial.println(_deviceNameString);
    
    
    const char* a8 = myObject["uuid"];
    if (a8)
    {
        _uuidString = const_cast<char*>(a8);
        _uuidString = createCopy(_uuidString);
    }
    
    
    const char* a9 = myObject["mqtt_topic"];
    if (a9)
    { _mqttTopicString = const_cast<char*>(a9);
        _mqttTopicString = createCopy(_mqttTopicString);
        Serial.println(_mqttTopicString);
    }
    
    
    const char* a10 = myObject["jsonHeader"];
    if (a10)
    { _jsonHeaderString = const_cast<char*>(a10);
        _jsonHeaderString = createCopy(_jsonHeaderString);
        Serial.println(_jsonHeaderString);
    }
    
    
    //Note: This is where the code could look for backward compatability, etc..
    const char* a11 = myObject["jsonVersion"];
    if (a11)
    { _jsonVersionString = const_cast<char*>(a11);
        _jsonVersionString = createCopy(_jsonVersionString);
        Serial.println(_jsonVersionString);
    }
    
}

//!process the JSON message (looking for FEED, etc)
int processJSONMessage(char *ascii)
{
    Serial.printf("processJSONMessage: %s\n", ascii);
    
    if (!(ascii[0] == '{'))
    {
        Serial.printf("processJSONMessage(%s) -> return false\n", ascii);
        
        return false;
    }
    
    // Deserialize the JSON document
    JSONVar myObject = JSON.parse(ascii);
    
    //open the preferences
    _preferences.begin("ESP32", false);
    Serial.print("Writing EPROM JSON = ");
    Serial.println(ascii);
    //save in EPROM
    _preferences.putString(_preferencesJSONName, ascii);
    
    // Close the Preferences
    _preferences.end();

//note: this just reboots after storing data in the EPROM .. question: is ESP generic enough..
        ESP.restart();

#ifdef JUST_REBOOT
    //NOTE: here is where different reasons for the info could be provided in the data>
    // eg. dataKind (wifi, mqtt, etc...)boot
    /**
     {
     "ssid" : "SunnyWhiteriver",
     "ssidPassword" : "sunny2021",
     
     "mqtt_user" : "idogwatch",
     "deviceName" : "HowieFeeder",
     "mqtt_password" : "password..",
     "uuid" : "scott",
     "mqtt_port" : "1883",
     "mqtt_server" : "knowledgeshark.me",
     "mqtt_status" : "Success"
     }
     */
    //TODO: parse the string...
    // this is info for the wifi
    // Store wifi config.  存储wifi配置信息
    //https://arduinojson.org
    
    const char* a1 = myObject["ssid"];
    if (a1)
    {
        _ssidString = const_cast<char*>(a1);
        _ssidString = createCopy(_ssidString);
    }
    
    const char* a2 = myObject["ssidPassword"];
    if (a2)
    {
        _ssidPasswordString = const_cast<char*>(a2);
        _ssidPasswordString = createCopy(_ssidPasswordString);
    }
    
    //the MQTT host/port/user/password  (topic is created in this code...)
    const char* a3 = myObject["mqtt_server"];
    if (a3)
    {
        _mqttServerString = const_cast<char*>(a3);
        _mqttServerString = createCopy(_mqttServerString);
    }
    
    const char* a4 = myObject["mqtt_port"];
    if (a4)
    {
        _mqttPortString = const_cast<char*>(a4);
        _mqttPortString = createCopy(_mqttPortString);
    }
    
    const char* a5 = myObject["mqtt_password"];
    if (a5)
    {
        _mqttPasswordString = const_cast<char*>(a5);
        _mqttPasswordString = createCopy(_mqttPasswordString);
    }
    
    const char* a6 = myObject["mqtt_user"];
    if (a6)
    {
        _mqttUserString = const_cast<char*>(a6);
        _mqttUserString = createCopy(_mqttUserString);
    }
    
    const char* a7 = myObject["deviceName"];
    if (a7)
    {
        _deviceNameString = const_cast<char*>(a7);
        _deviceNameString = createCopy(_deviceNameString);
    }
    
    const char* a8 = myObject["uuid"];
    if (a8)
    {
        _uuidString = const_cast<char*>(a8);
        _uuidString = createCopy(_uuidString);
    }
    
    const char* a9 = myObject["mqtt_topic"];
    if (a9)
    {
        _mqttTopicString = const_cast<char*>(a9);
        _mqttTopicString = createCopy(_mqttTopicString);
    }
    
    Serial.println("Write to eprom done!");

    
    //setup the WIFI if the ssid string (at least) is specified
    if (!isEmptyString(_ssidString ))
    {
        Serial.println("Setting WIFI from JSON parameters");
        setupWIFI(_ssidString, _ssidPasswordString);
    }
    
    //setup the MQTT if the server string is specified (at least)
    if (!isEmptyString(_mqttServerString))
    {
        Serial.println("Setting MQTT from JSON parameters");
        setupMQTT(_mqttServerString, _mqttPortString, _mqttPasswordString, _mqttUserString, _deviceNameString, _uuidString);
    }
  #endif  
    return true;
}
