/*
  Mecanum Wheel Remote Control - ESP-NOW Callbacks
  b_callbacks.ino
  Callbacks for communication with mecanum wheel robot car
  
  DroneBot Workshop 2022
  https://dronebotworkshop.com
*/
#include <Arduino.h>
#include <espnow.h>
#include "messages.h"
#include "callbacks.h"

// Pointer to received data
static struct_message_rcv *rcvData_p;
static size_t rcvDataSize = 0;
static volatile bool *wifiConnectError;     // pointer to overall indication of whether there is a connection error (FALSE is good)



// Setup needed callback function data
void callbacksInit( struct_message_rcv *ptr, size_t size, volatile bool *connectError )
{
    rcvData_p = ptr;
    rcvDataSize = size;
    wifiConnectError = connectError;
}

// Callback function called when data is sent
void OnDataSent( uint8_t *mac_addr, uint8_t  status )
{
    if( status == 0 )
    {
        *wifiConnectError = false;
        Serial.println("Message sent successfully!");
    }
    else
    {
        *wifiConnectError = true;
        Serial.println("Message send error!");
    }

    return;
}

// // Callback function executed when data is received
// void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
// {
//     // Get receievd data
//     memcpy(rcvData_p, incomingData, rcvDataSize );
// }