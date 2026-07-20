#include "packet_receiver.h"

#include <string.h>

void dek_packet_receiver_init(
    dek_packet_receiver_t *receiver)
{
    memset(receiver, 0, sizeof(*receiver));
}

bool dek_packet_receiver_feed(
    dek_packet_receiver_t *receiver,
    uint8_t byte,
    dek_packet_t *packet)
{
    /* Synchronize on first magic byte */

    if (receiver->index == 0)
    {
        if (byte != DEK_PACKET_MAGIC_BYTE0)
        {
            return false;
        }
    }

    /* Synchronize on second magic byte */

    if (receiver->index == 1)
    {
        if (byte != DEK_PACKET_MAGIC_BYTE1)
        {
            receiver->index = 0;
            return false;
        }
    }

    receiver->buffer[receiver->index++] = byte;

    /*
     * Once we have the header we know
     * exactly how long the packet is.
     */

    if (receiver->index == DEK_PACKET_HEADER_SIZE)
    {
        uint16_t payload_length =
            (uint16_t)receiver->buffer[DEK_PACKET_OFFSET_PAYLOAD_LENGTH] |
            ((uint16_t)receiver->buffer[DEK_PACKET_OFFSET_PAYLOAD_LENGTH + 1] << 8);

        receiver->expected_size =
            DEK_PACKET_OVERHEAD +
            payload_length;

        if (receiver->expected_size > DEK_MAX_PACKET_SIZE)
        {
            receiver->index = 0;
            receiver->expected_size = 0;
            return false;
        }
    }

    if (receiver->expected_size != 0 &&
        receiver->index == receiver->expected_size)
    {
        bool ok =
            dek_packet_decode(
                packet,
                receiver->buffer,
                receiver->expected_size);

        receiver->index = 0;
        receiver->expected_size = 0;

        return ok;
    }

    return false;
}