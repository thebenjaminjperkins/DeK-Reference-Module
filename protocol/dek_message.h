#ifndef DEK_MESSAGE_H
#define DEK_MESSAGE_H

#include <stdint.h>
#include "messages/dek_hello.h"
/*
 * DeK Transport Message Types
 *
 * These values are part of the on-wire protocol and must remain stable.
 */

 #define DEK_PROTOCOL_VERSION 1

typedef enum
{
    DEK_MSG_HELLO = 0x01,
    DEK_MSG_HELLO_ACK = 0x02,

    DEK_MSG_GET_DESCRIPTOR = 0x03,
    DEK_MSG_DESCRIPTOR = 0x04,

    DEK_MSG_GET_CAPABILITIES = 0x05,
    DEK_MSG_CAPABILITIES = 0x06,

    DEK_MSG_OPEN_CAPABILITY = 0x07,
    DEK_MSG_OPEN_ACK = 0x08,

    DEK_MSG_CLOSE_CAPABILITY = 0x09,
    DEK_MSG_CLOSE_ACK = 0x0A,

    DEK_MSG_COMMAND = 0x0B,
    DEK_MSG_RESPONSE = 0x0C,

    DEK_MSG_STREAM_DATA = 0x0D,
    DEK_MSG_EVENT = 0x0E,

    DEK_MSG_PING = 0x0F,
    DEK_MSG_PONG = 0x10,

    DEK_MSG_ERROR = 0x11,

    DEK_MSG_POLL = 0x12,
    DEK_MSG_EMPTY = 0x13

} dek_message_type_t;

/*
 * Transport Flags
 */

typedef enum
{
    DEK_FLAG_NONE              = 0x00,
    DEK_FLAG_RESPONSE_REQUIRED = 0x01,
    DEK_FLAG_MORE_PENDING      = 0x02,
    DEK_FLAG_ERROR             = 0x04,
    DEK_FLAG_STREAM            = 0x08

} dek_message_flag_t;

#endif