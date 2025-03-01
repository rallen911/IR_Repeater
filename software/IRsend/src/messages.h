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
    bool newMessage;    // Gets set to true when a new message has been received
    MESSAGE_TYPE_E msg_type;
    decode_results IRmessage_data;
} struct_message_rcv;