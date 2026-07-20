#ifndef DEK_PACKET_RECEIVER_H
#define DEK_PACKET_RECEIVER_H

#include <stdbool.h>
#include <stdint.h>

#include "dek_packet.h"

#define DEK_MAX_PACKET_SIZE 512

typedef struct
{
    uint8_t buffer[DEK_MAX_PACKET_SIZE];

    uint16_t index;

    uint16_t expected_size;

} dek_packet_receiver_t;

void dek_packet_receiver_init(
    dek_packet_receiver_t *receiver);

bool dek_packet_receiver_feed(
    dek_packet_receiver_t *receiver,
    uint8_t byte,
    dek_packet_t *packet);

#endif