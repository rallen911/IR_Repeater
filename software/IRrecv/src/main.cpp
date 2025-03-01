/* 
 *  IRrecv:  main.cpp - This file contains the high-level functions of the IR receiver portion of the IR_Repeater project.
*/
#include <Arduino.h>
#include <assert.h>
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

// The GPIO an IR detector/demodulator is connected to. Recommended: 14 (D5)
// Note: GPIO 16 won't work on the ESP8266 as it does not have interrupts.
// Note: GPIO 14 won't work on the ESP32-C3 as it causes the board to reboot.
#ifdef ARDUINO_ESP32C3_DEV
const uint16_t kRecvPin = 10;  // 14 on a ESP32-C3 causes a boot loop.
#else  // ARDUINO_ESP32C3_DEV
const uint16_t kRecvPin = 14;
#endif  // ARDUINO_ESP32C3_DEV

// The Serial connection baud rate.
// NOTE: Make sure you set your Serial Monitor to the same speed.
const uint32_t kBaudRate = 115200;

// As this program is a special purpose capture/resender, let's use a larger
// than expected buffer so we can handle very large IR messages.
const uint16_t kCaptureBufferSize = 1024;  // 1024 == ~511 bits

// kTimeout is the Nr. of milli-Seconds of no-more-data before we consider a
// message ended.
// This parameter is an interesting trade-off. The longer the timeout, the more
// complex a message it can capture. e.g. Some device protocols will send
// multiple message packets in quick succession, like Air Conditioner remotes.
// Air Coniditioner protocols often have a considerable gap (20-40+ms) between
// packets.
// The downside of a large timeout value is a lot of less complex protocols
// send multiple messages when the remote's button is held down. The gap between
// them is often also around 20+ms. This can result in the raw data be 2-3+
// times larger than needed as it has captured 2-3+ messages in a single
// capture. Setting a low timeout value can resolve this.
// So, choosing the best kTimeout value for your use particular case is
// quite nuanced. Good luck and happy hunting.
// NOTE: Don't exceed kMaxTimeoutMs. Typically 130ms.
const uint8_t kTimeout = 15;

// Set the smallest sized "UNKNOWN" message packets we actually care about.
// This value helps reduce the false-positive detection rate of IR background
// noise as real messages. The chances of background IR noise getting detected
// as a message increases with the length of the kTimeout value. (See above)
// The downside of setting this message too large is you can miss some valid
// short messages for protocols that this library doesn't yet decode.
//
// Set higher if you get lots of random short UNKNOWN messages when nothing
// should be sending a message.
// Set lower if you are sure your setup is working, but it doesn't see messages
// from your device. (e.g. Other IR remotes work.)
// NOTE: Set this value very high to effectively turn off UNKNOWN detection.
const uint16_t kMinUnknownSize = 12;

// How much percentage lee way do we give to incoming signals in order to match
// it?
// e.g. +/- 25% (default) to an expected value of 500 would mean matching a
//      value between 375 & 625 inclusive.
// Note: Default is 25(%). Going to a value >= 50(%) will cause some protocols
//       to no longer match correctly. In normal situations you probably do not
//       need to adjust this value. Typically that's when the library detects
//       your remote's message some of the time, but not all of the time.
const uint8_t kTolerancePercentage = kTolerance;  // kTolerance is normally 25%

// Legacy (No longer supported!)
//
// Change to `true` if you miss/need the old "Raw Timing[]" display.
#define LEGACY_TIMING_INFO false
// ==================== end of TUNEABLE PARAMETERS ====================

// The IR receiver.
IRrecv irrecv(kRecvPin, kCaptureBufferSize, kTimeout, true);

// ==================== begin of WiFi related data ====================
// MAC Address of responder - edit as required
uint8_t broadcastAddress[] = 
{
  0xC8, 0x2E, 0x18, 0xED, 0xED, 0x50
};

// Create a structured object for received data
struct_message_rcv rcvData;

// Create a structured object for sent data
struct_message_xmit xmitData;

// ESP-NOW Peer info
esp_now_peer_info_t peerInfo;

// Variable for connection error  - true is error state
static volatile bool wifiConnectError = true;

// This section of code runs only once at start-up.
void setup()
{
    Serial.begin(kBaudRate, SERIAL_8N1);

    while (!Serial)  // Wait for the serial connection to be establised.
        delay(50);
    
    Serial.println();

    // Perform a low level sanity checks that the compiler performs bit field
    // packing as we expect and Endianness is as we expect.
    assert(irutils::lowLevelSanityCheck() == 0);

    Serial.printf("\n" D_STR_IRRECVDUMP_STARTUP "\n", kRecvPin);
    // Ignore messages with less than minimum on or off pulses.
    irrecv.setUnknownThreshold(kMinUnknownSize);
    irrecv.setTolerance(kTolerancePercentage);  // Override the default tolerance.
    irrecv.enableIRIn();  // Start the receiver
    
    // Read the local MAC address and print it out.
    Serial.print("IRrecv MAC Address: ");
    Serial.println( WiFi.macAddress());

    // Set ESP32 as a Wi-Fi Station
    WiFi.mode(WIFI_STA);

    // Disable WiFi Sleep mode
    WiFi.setSleep(false);

    // Initilize ESP-NOW
    if( esp_now_init() != ESP_OK )
    {
        wifiConnectError = true;
    }
    else
    {
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
    if( esp_now_add_peer(&peerInfo) != ESP_OK )
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
    
    callbacksInit( &rcvData, sizeof(rcvData), &wifiConnectError );

    // Enter the Loop with connectError set HIGH to avoid intial display flicker
    wifiConnectError = false;
}

// The repeating section of the code
void loop()
{
    static uint32_t last_time = 0;
    uint32_t now;
    static uint32_t heartbeatTime = 0;
    
    now = millis();     // get current time
    heartbeatTime += now - last_time;
    last_time = now;    // save for next loop    

    // Check if an IR message has been received.
    if( irrecv.decode( &(xmitData.IRmessage_data)))
    {  // We have captured something.
        // The capture has stopped at this point.
        decode_type_t protocol = xmitData.IRmessage_data.decode_type;
        uint16_t size = xmitData.IRmessage_data.bits;
        bool success = true;

        // send IR data via WiFi to IRsend
        // Send message via ESP-NOW
        xmitData.msg_type = MSG_IR;
        esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)&xmitData, sizeof(xmitData));

        // Resume capturing IR messages. It was not restarted until after we sent
        // the message so we didn't capture our own message.
        irrecv.resume();

        // Display a crude timestamp & notification.
        Serial.printf(
            "%06u.%03u: A %d-bit %s message was %ssuccessfully retransmitted.\n",
            now / 1000, now % 1000, size, typeToString(protocol).c_str(),
            success ? "" : "un");
    }
  
    yield();  // Or delay(milliseconds); This ensures the ESP doesn't WDT reset.
}