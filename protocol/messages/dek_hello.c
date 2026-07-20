#include "dek_hello.h"
#include "dek_message.h"
#include <stddef.h>

void dek_hello_payload_init(dek_hello_payload_t *payload)
{
    if (payload == NULL)
    {
        return;
    }

    payload->min_protocol_version = DEK_PROTOCOL_VERSION;
    payload->max_protocol_version = DEK_PROTOCOL_VERSION;
    payload->host_flags = 0;
}
