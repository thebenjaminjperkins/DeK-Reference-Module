#include "packet_receiver.h"

#include <string.h>

/*
 * Reset all parser state so the receiver starts in a clean "waiting for sync"
 * condition. This is used both at startup and after malformed packets are
 * discarded.
 */
void dek_packet_receiver_init(
    dek_packet_receiver_t *receiver)
{
    memset(receiver, 0, sizeof(*receiver));
}

/*
 * Feed one byte from the UART stream into the packet receiver state machine.
 *
 * The receiver stays synchronized by requiring the two magic bytes at the
 * start of every packet. Once the fixed-size header has been collected, the
 * payload length field tells us how many more bytes are required before the
 * full packet can be validated and decoded.
 *
 * Returns true only when a complete, CRC-valid packet has been assembled and
 * written into `packet`. Returns false while a packet is still in progress or
 * when the current byte sequence is rejected.
 */
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
     * After the header arrives we can derive the total packet size from the
     * encoded payload length field. That lets the parser switch from
     * "searching" mode into "collect exactly N bytes" mode.
     */

    if (receiver->index == DEK_PACKET_HEADER_SIZE)
    {
        uint16_t payload_length =
            (uint16_t)receiver->buffer[DEK_PACKET_OFFSET_PAYLOAD_LENGTH] |
            ((uint16_t)receiver->buffer[DEK_PACKET_OFFSET_PAYLOAD_LENGTH + 1] << 8);

        receiver->expected_size =
            DEK_PACKET_OVERHEAD +
            payload_length;

        /*
         * Reject impossible packet sizes immediately so a corrupted header does
         * not cause buffer overrun or leave the parser wedged waiting for bytes
         * that will never fit.
         */
        if (receiver->expected_size > DEK_MAX_PACKET_SIZE)
        {
            receiver->index = 0;
            receiver->expected_size = 0;
            return false;
        }
    }

    /*
     * Once the expected number of bytes has been collected, hand the buffered
     * packet to the transport decoder. The internal state is always reset after
     * this point so the next byte can begin a fresh packet immediately.
     */
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
