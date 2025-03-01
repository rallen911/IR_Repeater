/*
  Mecanum Wheel Remote Control - ESP-NOW Callbacks
  callbacks.h
  Callbacks for communication with mecanum wheel robot car
  
  DroneBot Workshop 2022
  https://dronebotworkshop.com
*/
#include <esp_now.h>

void callbacksInit( struct_message_rcv *, size_t, volatile bool *, volatile bool * );
void OnDataSent( const uint8_t *, esp_now_send_status_t status );
void OnDataRecv( const uint8_t *mac, const uint8_t *incomingData, int len );