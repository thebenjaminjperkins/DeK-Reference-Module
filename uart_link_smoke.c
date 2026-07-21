#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"

#include <dek_protocol/dek_message.h>
#include <dek_protocol/dek_packet.h>
#include <dek_protocol/dek_receiver.h>
#include <dek_protocol/dek_transport.h>
#include <dek_protocol/message-types/dek_hello.h>

#define DEK_LINK_UART uart0
#define DEK_LINK_UART_ID 0u
#define DEK_LINK_UART_TX_PIN 0u
#define DEK_LINK_UART_RX_PIN 1u
#define DEK_LINK_UART_BAUD 115200u

#define DEK_STARTUP_DELAY_MS 2000u
#define DEK_STATS_INTERVAL_MS 2000u
#define DEK_PROTOCOL_RX_BUFFER_SIZE 512u
#define DEK_TX_BUFFER_SIZE 256u

typedef struct
{
    uint8_t selected_protocol_version;
    uint8_t module_flags;
    uint16_t reserved;
} dek_hello_ack_payload_t;

typedef struct
{
    dek_transport_t transport;
    dek_packet_receiver_t receiver;

    uint8_t receiver_buffer[DEK_PROTOCOL_RX_BUFFER_SIZE];

    uint32_t uart_bytes_received;
    uint32_t uart_bytes_transmitted;
    uint32_t packets_dispatched;
    uint32_t hello_requests;
    uint32_t hello_acks_sent;
    uint32_t command_requests;
    uint32_t responses_sent;
    uint32_t unsupported_messages;
    uint32_t receiver_invalid_packets;
    uint32_t receiver_buffer_overflows;
    uint32_t dispatch_errors;
    uint32_t last_stats_report_ms;
} dek_module_state_t;

static bool encode_hello_ack_payload(
    const dek_hello_ack_payload_t *hello_ack,
    uint8_t *buffer,
    uint16_t buffer_size)
{
    if (hello_ack == NULL || buffer == NULL || buffer_size < DEK_HELLO_PAYLOAD_SIZE)
    {
        return false;
    }

    buffer[0] = hello_ack->selected_protocol_version;
    buffer[1] = hello_ack->module_flags;
    buffer[2] = (uint8_t)(hello_ack->reserved & 0xFFu);
    buffer[3] = (uint8_t)((hello_ack->reserved >> 8) & 0xFFu);

    return true;
}

static void print_ascii_payload(
    const uint8_t *payload,
    uint16_t payload_length)
{
    printf("\"");

    for (uint16_t i = 0; i < payload_length; ++i)
    {
        uint8_t byte = payload[i];

        if (byte >= 32u && byte <= 126u)
        {
            printf("%c", (char)byte);
        }
        else
        {
            printf(".");
        }
    }

    printf("\"");
}

static void uart_link_init(void)
{
    uart_init(DEK_LINK_UART, DEK_LINK_UART_BAUD);
    gpio_set_function(DEK_LINK_UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(DEK_LINK_UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(DEK_LINK_UART, false, false);
    uart_set_format(DEK_LINK_UART, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(DEK_LINK_UART, true);
}

static bool uart_send_packet(
    dek_module_state_t *state,
    const uint8_t *buffer,
    uint16_t length,
    const char *label)
{
    if (state == NULL || buffer == NULL || length == 0u)
    {
        return false;
    }

    uart_write_blocking(DEK_LINK_UART, buffer, length);
    state->uart_bytes_transmitted += length;

    printf("%s TX (%u bytes):", label, length);

    for (uint16_t i = 0; i < length; ++i)
    {
        printf(" %02X", buffer[i]);
    }

    printf("\n");
    return true;
}

static bool protocol_versions_compatible(const dek_hello_payload_t *hello)
{
    return hello != NULL &&
           hello->min_protocol_version <= DEK_PROTOCOL_VERSION &&
           hello->max_protocol_version >= DEK_PROTOCOL_VERSION;
}

static bool send_hello_ack(
    dek_module_state_t *state,
    uint16_t channel)
{
    dek_hello_ack_payload_t hello_ack = {
        .selected_protocol_version = DEK_PROTOCOL_VERSION,
        .module_flags = 0u,
        .reserved = 0u,
    };
    uint8_t payload[DEK_HELLO_PAYLOAD_SIZE];
    uint8_t packet_buffer[DEK_TX_BUFFER_SIZE];
    uint16_t encoded_length = 0u;

    if (!encode_hello_ack_payload(&hello_ack, payload, sizeof(payload)))
    {
        return false;
    }

    if (!dek_transport_send(
            &state->transport,
            DEK_MSG_HELLO_ACK,
            channel,
            payload,
            sizeof(payload),
            packet_buffer,
            sizeof(packet_buffer),
            &encoded_length))
    {
        return false;
    }

    state->hello_acks_sent++;
    return uart_send_packet(state, packet_buffer, encoded_length, "HELLO_ACK");
}

static bool send_response(
    dek_module_state_t *state,
    uint16_t channel,
    const uint8_t *payload,
    uint16_t payload_length)
{
    uint8_t packet_buffer[DEK_TX_BUFFER_SIZE];
    uint16_t encoded_length = 0u;

    if (!dek_transport_send(
            &state->transport,
            DEK_MSG_RESPONSE,
            channel,
            payload,
            payload_length,
            packet_buffer,
            sizeof(packet_buffer),
            &encoded_length))
    {
        return false;
    }

    state->responses_sent++;
    return uart_send_packet(state, packet_buffer, encoded_length, "RESPONSE");
}

static bool handle_hello_packet(
    dek_module_state_t *state,
    const dek_packet_t *packet)
{
    dek_hello_payload_t hello;

    if (packet->header.payload_length != DEK_HELLO_PAYLOAD_SIZE)
    {
        printf(
            "HELLO rejected: payload length %u did not match %u\n",
            packet->header.payload_length,
            DEK_HELLO_PAYLOAD_SIZE);
        return false;
    }

    if (!dek_hello_decode(
            &hello,
            packet->payload,
            packet->header.payload_length))
    {
        printf("HELLO rejected: payload decode failed\n");
        return false;
    }

    state->hello_requests++;

    printf(
        "HELLO received: seq=%u channel=%u min=%u max=%u flags=0x%04X\n",
        packet->header.sequence_number,
        packet->header.channel_id,
        hello.min_protocol_version,
        hello.max_protocol_version,
        hello.host_flags);

    if (!protocol_versions_compatible(&hello))
    {
        printf(
            "HELLO rejected: local protocol version %u is outside requester range [%u, %u]\n",
            DEK_PROTOCOL_VERSION,
            hello.min_protocol_version,
            hello.max_protocol_version);
        return false;
    }

    return send_hello_ack(state, packet->header.channel_id);
}

static bool handle_command_packet(
    dek_module_state_t *state,
    const dek_packet_t *packet)
{
    state->command_requests++;

    printf(
        "COMMAND received: seq=%u channel=%u payload=%u ",
        packet->header.sequence_number,
        packet->header.channel_id,
        packet->header.payload_length);
    print_ascii_payload(packet->payload, packet->header.payload_length);
    printf("\n");

    return send_response(
        state,
        packet->header.channel_id,
        packet->payload,
        packet->header.payload_length);
}

static bool dispatch_packet(
    dek_module_state_t *state,
    const dek_packet_t *packet)
{
    state->packets_dispatched++;

    printf(
        "RX packet: type=%u seq=%u channel=%u payload=%u\n",
        packet->header.message_type,
        packet->header.sequence_number,
        packet->header.channel_id,
        packet->header.payload_length);

    switch ((dek_message_type_t)packet->header.message_type)
    {
        case DEK_MSG_HELLO:
            return handle_hello_packet(state, packet);

        case DEK_MSG_COMMAND:
            return handle_command_packet(state, packet);

        default:
            state->unsupported_messages++;
            printf("Unsupported message type: %u\n", packet->header.message_type);
            return false;
    }
}

static void handle_receiver_status(
    dek_module_state_t *state,
    dek_receiver_feed_status_t status)
{
    switch (status)
    {
        case DEK_RECEIVER_FEED_STATUS_INVALID_PACKET:
            state->receiver_invalid_packets++;
            printf("Receiver rejected malformed UART packet bytes\n");
            break;

        case DEK_RECEIVER_FEED_STATUS_BUFFER_OVERFLOW:
            state->receiver_buffer_overflows++;
            printf("Receiver buffer overflow while assembling UART packet\n");
            break;

        default:
            break;
    }
}

static void process_uart_rx(dek_module_state_t *state)
{
    while (uart_is_readable(DEK_LINK_UART))
    {
        uint8_t byte = (uint8_t)uart_getc(DEK_LINK_UART);
        dek_packet_t packet = { 0 };
        dek_receiver_feed_status_t status;

        state->uart_bytes_received++;
        status = dek_packet_receiver_feed(&state->receiver, byte, &packet);

        if (status == DEK_RECEIVER_FEED_STATUS_PACKET_READY)
        {
            state->transport.packets_received++;

            if (!dispatch_packet(state, &packet))
            {
                state->dispatch_errors++;
            }
        }
        else
        {
            handle_receiver_status(state, status);
        }
    }
}

static void report_stats(const dek_module_state_t *state)
{
    printf(
        "[uart-module] bytes_rx=%lu bytes_tx=%lu packets_rx=%lu packets_tx=%lu "
        "dispatched=%lu hello_req=%lu hello_ack=%lu command_req=%lu response=%lu "
        "unsupported=%lu dispatch_err=%lu rx_invalid=%lu rx_overflow=%lu\n",
        (unsigned long)state->uart_bytes_received,
        (unsigned long)state->uart_bytes_transmitted,
        (unsigned long)state->transport.packets_received,
        (unsigned long)state->transport.packets_sent,
        (unsigned long)state->packets_dispatched,
        (unsigned long)state->hello_requests,
        (unsigned long)state->hello_acks_sent,
        (unsigned long)state->command_requests,
        (unsigned long)state->responses_sent,
        (unsigned long)state->unsupported_messages,
        (unsigned long)state->dispatch_errors,
        (unsigned long)state->receiver_invalid_packets,
        (unsigned long)state->receiver_buffer_overflows);
}

static void maybe_report_stats(dek_module_state_t *state)
{
    uint32_t now_ms = to_ms_since_boot(get_absolute_time());

    if ((now_ms - state->last_stats_report_ms) < DEK_STATS_INTERVAL_MS)
    {
        return;
    }

    state->last_stats_report_ms = now_ms;
    report_stats(state);
}

int main(void)
{
    dek_module_state_t state;

    stdio_init_all();
    sleep_ms(DEK_STARTUP_DELAY_MS);

    memset(&state, 0, sizeof(state));
    dek_transport_init(&state.transport);
    dek_packet_receiver_init(
        &state.receiver,
        state.receiver_buffer,
        sizeof(state.receiver_buffer));

    uart_link_init();

    printf("DeK Reference Module UART bring-up starting on Tuesday, July 21, 2026\n");
    printf(
        "Link UART%u TX=%u RX=%u baud=%u\n",
        (unsigned int)DEK_LINK_UART_ID,
        (unsigned int)DEK_LINK_UART_TX_PIN,
        (unsigned int)DEK_LINK_UART_RX_PIN,
        (unsigned int)DEK_LINK_UART_BAUD);
    printf("USB serial monitor remains the active debug output\n");

    while (true)
    {
        process_uart_rx(&state);
        maybe_report_stats(&state);
        tight_loop_contents();
    }
}
