/* 
 *  IRsend:  main.cpp - This file contains the high-level functions of the IR transmitter portion of the IR_Repeater project.
*/
#include <Arduino.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRac.h>
#include <IRtext.h>
#include <IRutils.h>

// Include Libraries for ESP-NOW Communications
#include <esp_now.h>
#include <WiFi.h>
#include "messages.h"
#include "callbacks.h"

// ==================== start of TUNEABLE PARAMETERS ====================

// GPIO to use to control the IR LED circuit. Recommended: 19.
const uint16_t kIrLedPin = 19;

// The Serial connection baud rate.
// NOTE: Make sure you set your Serial Monitor to the same speed.
const uint32_t kBaudRate = 115200;

// As this program is a special purpose capture/resender, let's use a larger
// than expected buffer so we can handle very large IR messages.
const uint16_t kCaptureBufferSize = 1024;  // 1024 == ~511 bits

// kTimeout is the Nr. of milli-Seconds of no-more-data before we consider a
// message ended.
const uint8_t kTimeout = 50;  // Milli-Seconds

// kFrequency is the modulation frequency all UNKNOWN messages will be sent at.
const uint16_t kFrequency = 38000;  // in Hz. e.g. 38kHz.

// How much percentage lee way do we give to incoming signals in order to match
// it?
// e.g. +/- 25% (default) to an expected value of 500 would mean matching a
//      value between 375 & 625 inclusive.
// Note: Default is 25(%). Going to a value >= 50(%) will cause some protocols
//       to no longer match correctly. In normal situations you probably do not
//       need to adjust this value. Typically that's when the library detects
//       your remote's message some of the time, but not all of the time.
const uint8_t kTolerancePercentage = kTolerance;  // kTolerance is normally 25%

// ==================== end of TUNEABLE PARAMETERS ====================

// The IR transmitter.
IRsend irsend(kIrLedPin);

// ==================== begin of WiFi related data ====================
// MAC Address of responder - edit as required
uint8_t broadcastAddress[] = 
{
  0x08, 0xB6, 0x1F, 0xB8, 0x17, 0x10
};

// Create a structured object for received data
struct_message_rcv rcvData;

// ESP-NOW Peer info
esp_now_peer_info_t peerInfo;

// Variable for connection error  - true is error state
static volatile bool wifiConnectError = true;

// Variable to signal receipt of an IR message to decode / repeat
static volatile bool IRMessageReceived = false;

// Variable for connection status string
String connectStatus = "NO INFO";

// This section of code runs only once at start-up.
void setup()
{
    irsend.begin();       // Start up the IR sender.

    Serial.begin(kBaudRate, SERIAL_8N1);

    while (!Serial)  // Wait for the serial connection to be establised.
        delay(50);
    
    Serial.println();

    // Read the local MAC address and print it out.
    Serial.print("IRrecv MAC Address: ");
    Serial.println( WiFi.macAddress());

    // Set ESP32 as a Wi-Fi Station
    WiFi.mode(WIFI_STA);

    // Disable WiFi Sleep mode
    WiFi.setSleep(false);

    // Initilize ESP-NOW
    if (esp_now_init() != ESP_OK)
    {
        connectStatus = "ESP-NOW Error";
        wifiConnectError = true;
        return;
    }
    else
    {
        connectStatus = "ESP-NOW OK";
        wifiConnectError = false;
    }

    // Register receive callback function
    esp_now_register_recv_cb(OnDataRecv);

    // Register the send callback
    esp_now_register_send_cb(OnDataSent);

    // Register peer
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    // Add peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        Serial.println("No peer added");
        wifiConnectError = true;
        return;
    }
    else
    {
        Serial.println("ESP-NOW Ready");
        wifiConnectError = false;
    }
    
    Serial.println("SmartIRRepeater is now running and waiting for IR input on Pin ");

    callbacksInit( &rcvData, sizeof(rcvData), &wifiConnectError, &IRMessageReceived );

    // Enter the Loop with connectError set HIGH to avoid intial display flicker
    wifiConnectError = true;
}

// The repeating section of the code
void loop()
{
    static uint32_t last_time = 0;
    uint32_t now;
    static uint32_t heartbeatTime = 0;

    static uint8_t pinState = LOW;
    
    now = millis();     // get current time
    heartbeatTime += now - last_time;
    last_time = now;    // save for next loop    

    // Check connection status
    if( rcvData.newMessage == true )
    {
        if( pinState == LOW )
            pinState = HIGH;
        else
            pinState = LOW;
        
        digitalWrite( kIrLedPin, pinState );
        rcvData.newMessage = false;

        decode_type_t protocol = rcvData.IRmessage_data.decode_type;
        uint16_t size = rcvData.IRmessage_data.bits;
        bool success = true;

        Serial.printf(D_STR_TIMESTAMP " : %06u.%03u\n", now / 1000, now % 1000);

        // Check if we got an IR message that was to big for our capture buffer.
        if (rcvData.IRmessage_data.overflow)
            Serial.printf(D_WARN_BUFFERFULL "\n", kCaptureBufferSize);

        // Display the library version the message was captured with.
        Serial.println(D_STR_LIBRARY "   : v" _IRREMOTEESP8266_VERSION_STR "\n");

        // Display the tolerance percentage if it has been change from the default.
        if (kTolerancePercentage != kTolerance)
            Serial.printf(D_STR_TOLERANCE " : %d%%\n", kTolerancePercentage);

        // Display the basic output of what we found.
        Serial.print(resultToHumanReadableBasic(&rcvData.IRmessage_data));

        // Display any extra A/C info if we have it.
        String description = IRAcUtils::resultAcToString(&rcvData.IRmessage_data);

        if (description.length()) Serial.println(D_STR_MESGDESC ": " + description);

        yield();  // Feed the WDT as the text output can take a while to print.

		// Is it a protocol we don't understand?
        if (protocol == decode_type_t::UNKNOWN)
        {  // Yes.
            // Convert the results into an array suitable for sendRaw().
            // resultToRawArray() allocates the memory we need for the array.
            uint16_t *raw_array = resultToRawArray(&rcvData.IRmessage_data);
            // Find out how many elements are in the array.
            size = getCorrectedRawLength(&rcvData.IRmessage_data);
#if SEND_RAW
            // Send it out via the IR LED circuit.
            irsend.sendRaw(raw_array, size, kFrequency);
#endif  // SEND_RAW
            // Deallocate the memory allocated by resultToRawArray().
            delete [] raw_array;
        }
        else if( hasACState( protocol ))
        {  // Does the message require a state[]?
            // It does, so send with bytes instead.
            success = irsend.send(protocol, rcvData.IRmessage_data.state, size / 8);
        }
        else
        {  // Anything else must be a simple message protocol. ie. <= 64 bits
            success = irsend.send(protocol, rcvData.IRmessage_data.value, size);
        }
        
            // Display a crude timestamp & notification.
            Serial.printf(
                "%06u.%03u: A %d-bit %s message was %ssuccessfully retransmitted.\n",
                now / 1000, now % 1000, size, typeToString(protocol).c_str(),
                success ? "" : "un");
  
        yield();  // Or delay(milliseconds); This ensures the ESP doesn't WDT reset.
    }    
}