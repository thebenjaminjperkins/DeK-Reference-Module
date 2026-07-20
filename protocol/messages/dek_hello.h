#ifndef DEK_HELLO_H
#define DEK_HELLO_H

#include <stdint.h>

typedef struct
{
    uint8_t min_protocol_version;
    uint8_t max_protocol_version;
    uint16_t host_flags;
} dek_hello_payload_t;

void dek_hello_payload_init(dek_hello_payload_t *payload);

#endif