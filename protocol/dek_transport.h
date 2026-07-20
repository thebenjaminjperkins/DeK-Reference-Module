#ifndef DEK_TRANSPORT_H
#define DEK_TRANSPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "dek_message.h"
#include "dek_packet.h"

/* Monotonic sequence number assigned to each outbound packet. */
typedef struct
{
    uint16_t next_sequence_number;

    uint32_t request_timeout_ms;

    uint32_t packets_sent;
    uint32_t packets_received;

    uint32_t crc_errors;
    uint32_t timeout_errors;
    uint32_t malformed_packets;

} dek_transport_t;


/*
 * Initialize the transport layer state.
 *
 * The sequence counter is reset to 1 so the first transmission uses a
 * non-zero sequence number and subsequent sends remain ordered.
 */
void dek_transport_init(dek_transport_t *transport);

/*
 * Build and encode a transport packet from message metadata and payload bytes.
 *
 * Parameters:
 * - transport: transport state used to assign the outbound sequence number.
 * - message_type: logical message type written into the packet header.
 * - channel: target logical channel identifier for the packet.
 * - payload: pointer to the bytes that will be carried as the packet body.
 * - payload_length: number of bytes in the payload buffer.
 * - tx_buffer: destination buffer that receives the encoded packet bytes.
 * - tx_buffer_size: capacity of tx_buffer in bytes.
 *
 * Returns false when the transport state, output buffer, or packet encoding
 * prerequisites are invalid.
 */
bool dek_transport_send(
    dek_transport_t *transport,
    dek_message_type_t message_type,
    uint16_t channel,
    const uint8_t *payload,
    uint16_t payload_length,
    uint8_t *tx_buffer,
    uint16_t tx_buffer_size,
    uint16_t *encoded_length);

bool dek_transport_send_hello(
    dek_transport_t *transport,
    uint8_t *tx_buffer,
    uint16_t tx_buffer_size,
    uint16_t *encoded_length);

bool transport_hello_self_test(void);

    #endif