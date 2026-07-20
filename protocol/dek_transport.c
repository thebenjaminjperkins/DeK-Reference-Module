#include <stdio.h>

#include "dek_transport.h"
#include "messages/dek_hello.h"
/*
 * Reset the transport state before sending any packets.
 *
 * A starting sequence of 1 keeps the first packet distinguishable from an
 * uninitialized or zero-valued counter and preserves a simple monotonic order.
 */
void dek_transport_init(dek_transport_t *transport)
{
    if (transport == NULL)
    {
        return;
    }

    transport->next_sequence_number = 1;
}

/*
 * Encode a packet using the transport-layer metadata.
 *
 * This function fills the packet header with the message type, logical channel,
 * payload length, and a freshly assigned sequence number. The payload bytes are
 * passed through without copying so the caller retains ownership of the data.
 */
bool dek_transport_send(
    dek_transport_t *transport,
    dek_message_type_t message_type,
    uint16_t channel,
    const uint8_t *payload,
    uint16_t payload_length,
    uint8_t *tx_buffer,
    uint16_t tx_buffer_size,
    uint16_t *encoded_length)
{
    if (transport == NULL || tx_buffer == NULL)
    {
        return false;
    }

    dek_packet_t packet;

    /* Initialize the header to the default protocol values before populating it. */
    dek_packet_init(&packet.header);

    /* Populate the packet header with transport-layer and message-specific fields. */
    packet.header.message_type = message_type;
    packet.header.sequence_number = transport->next_sequence_number++;
    packet.header.channel_id = channel;
    packet.header.payload_length = payload_length;

    /* The payload is referenced directly by the encoder. */
    packet.payload = payload;

   if (!dek_packet_encode(
        &packet,
        tx_buffer,
        tx_buffer_size))
    {
        return false;
    }

    if (encoded_length != NULL)
    {
        *encoded_length =
            DEK_PACKET_OVERHEAD +
            payload_length;
    }

    return true;
}

bool dek_transport_send_hello(
    dek_transport_t *transport,
    uint8_t *tx_buffer,
    uint16_t tx_buffer_size,
    uint16_t *encoded_length) {
        dek_hello_payload_t hello;
        dek_hello_payload_init(&hello);
        return dek_transport_send(
            transport,
            DEK_MSG_HELLO,
            0,
            (const uint8_t *)&hello,
            sizeof(hello),
            tx_buffer,
            tx_buffer_size, encoded_length);
    }

bool transport_hello_self_test(void) {
    // Init transport
    dek_transport_t transport;
    dek_transport_init(&transport);
    uint8_t tx_buffer[256];
    uint16_t encoded_length;

    // Build HELLO packet
    if (!dek_transport_send_hello(
        &transport,
        tx_buffer,
        sizeof(tx_buffer),
        &encoded_length))
    {
        return false;
    }

    // Decode packet
    dek_packet_t decoded_packet;

    if (!dek_packet_decode(
            &decoded_packet,
            tx_buffer,
            encoded_length))
    {
        return false;
    }
    
    //Verify: Message Type, Channel, Payload, Length, Sequence Number, CRC
    if (decoded_packet.header.message_type != DEK_MSG_HELLO) {
        printf("Message type failed");
        return false;
    }

    if (decoded_packet.header.channel_id != 0) {
        printf("Channel ID failed");
        return false;
    }

    if (decoded_packet.header.payload_length != sizeof(dek_hello_payload_t)) {
        printf("Payload length failed");
        return false;
    }

    if (decoded_packet.header.sequence_number != 1) {
        printf("Sequence type failed");
        return false;
    }

    const dek_hello_payload_t *hello = (const dek_hello_payload_t *)decoded_packet.payload;
    if (hello->host_flags != 0) {
        printf("Host flags failed");
        return false;
    }
    
    if (hello->max_protocol_version != DEK_PROTOCOL_VERSION) {
        printf("Max protocol version failed");
        return false;
    }

    if (hello->min_protocol_version != DEK_PROTOCOL_VERSION) {
        printf("Min protocol version failed");
        return false;
    }


    // PASS
    printf("Transport HELLO self-test PASSED\n");
    return true;
    
}