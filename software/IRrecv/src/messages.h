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

// Create a structured object for sending IR data
typedef struct struct_IRmessage_xmit
{
    MESSAGE_TYPE_E msg_type;
    decode_results IRmessage_data;
} struct_IRmessage_xmit;

// Create a structured object for sending status data
typedef struct struct_message_xmit
{
    MESSAGE_TYPE_E msg_type;
    uint8_t status_data;
} struct_message_xmit;