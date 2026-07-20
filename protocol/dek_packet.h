
#ifndef DEK_PACKET_H
#define DEK_PACKET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define DEK_PACKET_MAGIC_BYTE0     0x44
#define DEK_PACKET_MAGIC_BYTE1     0x4B
#define DEK_PACKET_VERSION_V1      1
#define DEK_PACKET_HEADER_SIZE     12

#define DEK_PACKET_OFFSET_MAGIC             0
#define DEK_PACKET_OFFSET_PROTOCOL_VERSION  2
#define DEK_PACKET_OFFSET_MESSAGE_TYPE      3
#define DEK_PACKET_OFFSET_FLAGS             4
#define DEK_PACKET_OFFSET_HEADER_LENGTH     5
#define DEK_PACKET_OFFSET_SEQUENCE_NUMBER   6
#define DEK_PACKET_OFFSET_CHANNEL_ID        8
#define DEK_PACKET_OFFSET_PAYLOAD_LENGTH    10
#define DEK_PACKET_CRC_SIZE                 2
#define DEK_PACKET_OVERHEAD \
    (DEK_PACKET_HEADER_SIZE + DEK_PACKET_CRC_SIZE)

typedef struct
{
    uint8_t magic[2];

    uint8_t protocol_version;
    uint8_t message_type;
    uint8_t flags;
    uint8_t header_length;

    uint16_t sequence_number;
    uint16_t channel_id;
    uint16_t payload_length;

} dek_packet_header_t;

typedef struct 
{
    dek_packet_header_t header;
    const uint8_t *payload;
} dek_packet_t;

void dek_packet_init(dek_packet_header_t *header);

bool dek_packet_encode_header(
    const dek_packet_header_t *header,
    uint8_t *buffer,
    uint16_t buffer_size);

bool dek_packet_decode_header(
    dek_packet_header_t *header,
    const uint8_t *buffer,
    uint16_t buffer_size);

bool dek_packet_encode (
    const dek_packet_t *packet,
    uint8_t *buffer,
    uint16_t buffer_size);

bool dek_packet_decode (
    dek_packet_t *packet,
    const uint8_t *buffer,
    uint16_t buffer_size);

#endif