/** The MQTT + WIFI part
 
 
 Created: on Jan 1, 2022, iDogWatch.com
 Author: Scott Moody
 
 */
//#include <string>

    /*******************************MQTT*************************************/
    
    //!callback with the message if required (like sending the FEED message)
    //!!function pointers: https://www.cprogramming.com/tutorial/function-pointers.html
    //!define as: void callback(char* message)
    //!  call processMessage(message, &callback);
    //! This must be called first before the setup or loop
    //! eg:   void myCallback(char* message) { }
    //!    setMessageCallback(&myCallback)
    //void setMessageCallbacks(void (*callback)(char*), void (*blinkTheLED)());
    
#define CALLBACK_FEED 0
#define CALLBACK_BLINK 1
#define CALLBACK_TEMP 2
    //! register the callback function (with a char* message) with the callbackType (1,2,3...)
    //!  eg  registerCallback(CALLBACK_FEED, &messageFeedFunction)
    void registerCallback(int callbackType, void (*callback)(char*));
    
    //THIS IS the setup() and loop() but using the "component" name, eg MQTTNetworking()
    //! called from the setup()
    void setupMQTTNetworking();
    
    //! called for the loop() of this plugin
    void loopMQTTNetworking();
    
    //!called for things like the advertisement
    char *getDeviceName();
    
    //!process the JSON message, which can be configuration information. This is called from outside on things like a Bluetooth message..
    //!return true if valid JSON, and false otherwise. This looks for '{'
    int processJSONMessage(char *message);
    
    //!process an MQTT message looking for keywords (barklet language)
   // void processBarkletMessage(String message);
    
