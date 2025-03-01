#include <IRrecv.h>

typedef enum
{
    MSG_IR          = 0x00,
    MSG_IR_ACK      = 0x01,
    MSG_HEARTBEAT   = 0xFF
} MESSAGE_TYPE_E;

// Define a data structure for received data
typedef struct struct_message_rcv
{
    MESSAGE_TYPE_E msg_type;
} struct_message_rcv;

// Create a structured object for sent data
typedef struct struct_message_xmit
{
    MESSAGE_TYPE_E msg_type;
    decode_results IRmessage_data;
} struct_message_xmit;