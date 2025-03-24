/*
  Mecanum Wheel Remote Control - ESP-NOW Callbacks
  callbacks.h
  Callbacks for communication with mecanum wheel robot car
  
  DroneBot Workshop 2022
  https://dronebotworkshop.com
*/
#include <espnow.h>

void callbacksInit( struct_message_rcv *, size_t, volatile bool * );
void OnDataSent( uint8_t *, uint8_t status );
void OnDataRecv( uint8_t *mac, uint8_t *incomingData, uint8_t len );