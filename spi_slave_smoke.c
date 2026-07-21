#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"

#include <dek_protocol/dek_message.h>
#include <dek_protocol/dek_packet.h>
#include <dek_protocol/dek_receiver.h>
#include <dek_protocol/dek_transport.h>
#include <dek_protocol/message-types/dek_hello.h>

#if PICO_DEFAULT_SPI == 0
#define DEK_SPI_PORT spi0
#else
#define DEK_SPI_PORT spi1
#endif

#define DEK_SPI_INIT_BAUDRATE_HZ 1000000u
#define DEK_SPI_IDLE_BYTE 0x00u
#define DEK_STARTUP_DELAY_MS 2000u
#define DEK_STATS_INTERVAL_MS 2000u
#define DEK_PROTOCOL_RX_BUFFER_SIZE 512u
#define DEK_TX_QUEUE_SIZE (DEK_PROTOCOL_RX_BUFFER_SIZE * 2u)
#define DEK_SPI_INTERBYTE_TIMEOUT_US 1500u

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
    uint8_t tx_queue[DEK_TX_QUEUE_SIZE];

    uint16_t tx_head;
    uint16_t tx_tail;
    uint16_t tx_count;

    uint32_t spi_bytes_received;
    uint32_t spi_bytes_transmitted;
    uint32_t packets_dispatched;
    uint32_t hello_requests;
    uint32_t hello_acks_queued;
    uint32_t poll_requests;
    uint32_t unsupported_messages;
    uint32_t receiver_invalid_packets;
    uint32_t receiver_buffer_overflows;
    uint32_t response_queue_overruns;
    uint32_t dispatch_errors;
    uint32_t last_stats_report_ms;

    bool transaction_active;
    bool transaction_truncated;
    uint16_t transaction_length;
    uint32_t transaction_count;
    uint8_t transaction_bytes[64];
} dek_module_state_t;

static bool queue_push_bytes(
    dek_module_state_t *state,
    const uint8_t *data,
    uint16_t length)
{
    if (state == NULL || data == NULL)
    {
        return false;
    }

    if (length > (DEK_TX_QUEUE_SIZE - state->tx_count))
    {
        return false;
    }

    for (uint16_t i = 0; i < length; ++i)
    {
        state->tx_queue[state->tx_tail] = data[i];
        state->tx_tail = (uint16_t)((state->tx_tail + 1u) % DEK_TX_QUEUE_SIZE);
    }

    state->tx_count = (uint16_t)(state->tx_count + length);
    return true;
}

static uint8_t queue_pop_byte_or_idle(dek_module_state_t *state)
{
    uint8_t byte = DEK_SPI_IDLE_BYTE;

    if (state == NULL || state->tx_count == 0u)
    {
        return byte;
    }

    byte = state->tx_queue[state->tx_head];
    state->tx_head = (uint16_t)((state->tx_head + 1u) % DEK_TX_QUEUE_SIZE);
    state->tx_count--;

    return byte;
}

static void service_spi_tx_fifo(dek_module_state_t *state)
{
    while (spi_is_writable(DEK_SPI_PORT))
    {
        spi_get_hw(DEK_SPI_PORT)->dr = queue_pop_byte_or_idle(state);
    }
}

static bool spi_cs_is_active(void)
{
    return gpio_get(PICO_DEFAULT_SPI_CSN_PIN) == 0;
}

static void begin_spi_transaction(dek_module_state_t *state)
{
    state->transaction_active = true;
    state->transaction_truncated = false;
    state->transaction_length = 0u;
}

static void capture_spi_transaction_byte(
    dek_module_state_t *state,
    uint8_t byte)
{
    if (!state->transaction_active)
    {
        begin_spi_transaction(state);
    }

    if (state->transaction_length < sizeof(state->transaction_bytes))
    {
        state->transaction_bytes[state->transaction_length++] = byte;
    }
    else
    {
        state->transaction_truncated = true;
    }
}

static void finish_spi_transaction(dek_module_state_t *state)
{
    if (!state->transaction_active)
    {
        return;
    }

    state->transaction_active = false;
    state->transaction_count++;

    printf(
        "SPI transaction %lu captured %u byte(s)%s:",
        (unsigned long)state->transaction_count,
        (unsigned int)state->transaction_length,
        state->transaction_truncated ? " (truncated)" : "");

    for (uint16_t i = 0; i < state->transaction_length; ++i)
    {
        printf(" %02X", state->transaction_bytes[i]);
    }

    printf("\n");
}

static bool test_packet_round_trip(void)
{
    static const uint8_t payload[] = { 0x10u, 0x20u, 0x30u, 0x40u };

    uint8_t buffer[64];
    dek_packet_t packet = { 0 };
    dek_packet_t decoded = { 0 };

    dek_packet_init(&packet.header);
    packet.header.message_type = DEK_MSG_COMMAND;
    packet.header.flags = DEK_FLAG_RESPONSE_REQUIRED;
    packet.header.sequence_number = 7u;
    packet.header.channel_id = 3u;
    packet.header.payload_length = (uint16_t)sizeof(payload);
    packet.payload = payload;

    if (!dek_packet_encode(&packet, buffer, sizeof(buffer)))
    {
        return false;
    }

    if (!dek_packet_validate(
            buffer,
            dek_packet_encoded_size(sizeof(payload)),
            NULL))
    {
        return false;
    }

    if (!dek_packet_decode(
            &decoded,
            buffer,
            dek_packet_encoded_size(sizeof(payload))))
    {
        return false;
    }

    return decoded.header.message_type == DEK_MSG_COMMAND &&
           decoded.header.flags == DEK_FLAG_RESPONSE_REQUIRED &&
           decoded.header.sequence_number == 7u &&
           decoded.header.channel_id == 3u &&
           decoded.header.payload_length == sizeof(payload) &&
           memcmp(decoded.payload, payload, sizeof(payload)) == 0;
}

static bool test_hello_encode_decode(void)
{
    dek_hello_payload_t hello;
    dek_hello_payload_t decoded;
    uint8_t buffer[DEK_HELLO_PAYLOAD_SIZE];

    dek_hello_payload_init(&hello);
    hello.host_flags = 0x1234u;

    if (!dek_hello_encode(&hello, buffer, sizeof(buffer)))
    {
        return false;
    }

    if (!dek_hello_decode(&decoded, buffer, sizeof(buffer)))
    {
        return false;
    }

    return decoded.min_protocol_version == DEK_PROTOCOL_VERSION &&
           decoded.max_protocol_version == DEK_PROTOCOL_VERSION &&
           decoded.host_flags == 0x1234u;
}

static bool test_transport_hello(void)
{
    dek_transport_t transport;
    dek_packet_t packet = { 0 };
    dek_hello_payload_t hello;
    uint8_t tx_buffer[64];
    uint16_t encoded_length = 0u;

    dek_transport_init(&transport);

    if (!dek_transport_send_hello(
            &transport,
            tx_buffer,
            sizeof(tx_buffer),
            &encoded_length))
    {
        return false;
    }

    if (!dek_transport_receive(
            &transport,
            &packet,
            tx_buffer,
            encoded_length))
    {
        return false;
    }

    if (!dek_hello_decode(
            &hello,
            packet.payload,
            packet.header.payload_length))
    {
        return false;
    }

    return transport.packets_sent == 1u &&
           transport.packets_received == 1u &&
           packet.header.message_type == DEK_MSG_HELLO &&
           packet.header.sequence_number == 1u &&
           packet.header.channel_id == 0u &&
           packet.header.payload_length == DEK_HELLO_PAYLOAD_SIZE &&
           hello.min_protocol_version == DEK_PROTOCOL_VERSION &&
           hello.max_protocol_version == DEK_PROTOCOL_VERSION &&
           hello.host_flags == 0u;
}

static bool test_receiver_reassembles_packets(void)
{
    dek_transport_t transport;
    dek_packet_t packet = { 0 };
    dek_packet_receiver_t receiver;
    uint8_t receiver_buffer[64];
    uint8_t tx_buffer[64];
    uint16_t encoded_length = 0u;
    dek_receiver_feed_status_t status = DEK_RECEIVER_FEED_STATUS_SYNCING;

    dek_transport_init(&transport);
    dek_packet_receiver_init(&receiver, receiver_buffer, sizeof(receiver_buffer));

    if (!dek_transport_send_hello(
            &transport,
            tx_buffer,
            sizeof(tx_buffer),
            &encoded_length))
    {
        return false;
    }

    for (uint16_t i = 0; i < encoded_length; ++i)
    {
        status = dek_packet_receiver_feed(&receiver, tx_buffer[i], &packet);
    }

    return status == DEK_RECEIVER_FEED_STATUS_PACKET_READY &&
           packet.header.message_type == DEK_MSG_HELLO &&
           packet.header.payload_length == DEK_HELLO_PAYLOAD_SIZE;
}

static bool run_startup_self_tests(void)
{
    bool packet_ok = test_packet_round_trip();
    bool hello_ok = test_hello_encode_decode();
    bool transport_ok = test_transport_hello();
    bool receiver_ok = test_receiver_reassembles_packets();

    printf("Self-test packet: %s\n", packet_ok ? "PASS" : "FAIL");
    printf("Self-test hello: %s\n", hello_ok ? "PASS" : "FAIL");
    printf("Self-test transport: %s\n", transport_ok ? "PASS" : "FAIL");
    printf("Self-test receiver: %s\n", receiver_ok ? "PASS" : "FAIL");

    return packet_ok && hello_ok && transport_ok && receiver_ok;
}

static void spi_link_init(void)
{
    spi_init(DEK_SPI_PORT, DEK_SPI_INIT_BAUDRATE_HZ);
    spi_set_format(DEK_SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    spi_set_slave(DEK_SPI_PORT, true);
    spi_set_baudrate(DEK_SPI_PORT, DEK_SPI_INIT_BAUDRATE_HZ);

    gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(PICO_DEFAULT_SPI_CSN_PIN, GPIO_FUNC_SPI);

    while (spi_is_readable(DEK_SPI_PORT))
    {
        (void)spi_get_hw(DEK_SPI_PORT)->dr;
    }
}

static void report_stats(const dek_module_state_t *state)
{
    printf(
        "[stats] spi_rx=%lu spi_tx=%lu packets_rx=%lu packets_tx=%lu "
        "dispatched=%lu hello_req=%lu hello_ack=%lu poll_req=%lu unsupported=%lu "
        "dispatch_err=%lu queue_pending=%u queue_overrun=%lu txns=%lu "
        "rx_invalid=%lu rx_overflow=%lu\n",
        (unsigned long)state->spi_bytes_received,
        (unsigned long)state->spi_bytes_transmitted,
        (unsigned long)state->transport.packets_received,
        (unsigned long)state->transport.packets_sent,
        (unsigned long)state->packets_dispatched,
        (unsigned long)state->hello_requests,
        (unsigned long)state->hello_acks_queued,
        (unsigned long)state->poll_requests,
        (unsigned long)state->unsupported_messages,
        (unsigned long)state->dispatch_errors,
        (unsigned int)state->tx_count,
        (unsigned long)state->response_queue_overruns,
        (unsigned long)state->transaction_count,
        (unsigned long)state->receiver_invalid_packets,
        (unsigned long)state->receiver_buffer_overflows);
}

static bool protocol_versions_compatible(const dek_hello_payload_t *hello)
{
    return hello != NULL &&
           hello->min_protocol_version <= DEK_PROTOCOL_VERSION &&
           hello->max_protocol_version >= DEK_PROTOCOL_VERSION;
}

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

static bool queue_hello_ack(
    dek_module_state_t *state,
    uint16_t channel,
    uint16_t *encoded_length)
{
    dek_hello_ack_payload_t hello_ack = {
        .selected_protocol_version = DEK_PROTOCOL_VERSION,
        .module_flags = 0u,
        .reserved = 0u,
    };
    uint8_t hello_ack_payload[DEK_HELLO_PAYLOAD_SIZE];
    uint8_t encoded_packet[DEK_PROTOCOL_RX_BUFFER_SIZE];
    uint16_t response_length = 0u;

    if (!encode_hello_ack_payload(
            &hello_ack,
            hello_ack_payload,
            sizeof(hello_ack_payload)))
    {
        return false;
    }

    if (!dek_transport_send(
            &state->transport,
            DEK_MSG_HELLO_ACK,
            channel,
            hello_ack_payload,
            sizeof(hello_ack_payload),
            encoded_packet,
            sizeof(encoded_packet),
            &response_length))
    {
        return false;
    }

    if (!queue_push_bytes(
            state,
            encoded_packet,
            response_length))
    {
        state->response_queue_overruns++;
        return false;
    }

    if (encoded_length != NULL)
    {
        *encoded_length = response_length;
    }

    return true;
}

static bool handle_hello_packet(
    dek_module_state_t *state,
    const dek_packet_t *packet)
{
    dek_hello_payload_t hello;
    uint16_t encoded_length = 0u;

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

    if (!queue_hello_ack(
            state,
            packet->header.channel_id,
            &encoded_length))
    {
        printf("HELLO_ACK queue failed\n");
        return false;
    }

    state->hello_acks_queued++;

    printf(
        "HELLO_ACK queued: channel=%u bytes=%u\n",
        packet->header.channel_id,
        encoded_length);

    return true;
}

static bool handle_poll_packet(
    dek_module_state_t *state,
    const dek_packet_t *packet)
{
    state->poll_requests++;

    printf(
        "POLL received: seq=%u channel=%u pending_tx=%u\n",
        packet->header.sequence_number,
        packet->header.channel_id,
        state->tx_count);

    return true;
}

static bool dispatch_packet(
    dek_module_state_t *state,
    const dek_packet_t *packet)
{
    state->packets_dispatched++;

    printf(
        "Packet received: type=%u seq=%u channel=%u payload=%u\n",
        packet->header.message_type,
        packet->header.sequence_number,
        packet->header.channel_id,
        packet->header.payload_length);

    switch ((dek_message_type_t)packet->header.message_type)
    {
        case DEK_MSG_HELLO:
            return handle_hello_packet(state, packet);

        case DEK_MSG_POLL:
            return handle_poll_packet(state, packet);

        default:
            state->unsupported_messages++;
            printf(
                "Unsupported message type: %u\n",
                packet->header.message_type);
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
            printf("Receiver rejected malformed packet bytes\n");
            break;

        case DEK_RECEIVER_FEED_STATUS_BUFFER_OVERFLOW:
            state->receiver_buffer_overflows++;
            printf("Receiver buffer overflow while assembling packet\n");
            break;

        default:
            break;
    }
}

static bool process_spi_rx_byte(
    dek_module_state_t *state,
    uint8_t byte)
{
    dek_packet_t packet = { 0 };
    dek_receiver_feed_status_t status;

    status = dek_packet_receiver_feed(&state->receiver, byte, &packet);

    if (status == DEK_RECEIVER_FEED_STATUS_PACKET_READY)
    {
        state->transport.packets_received++;

        if (!dispatch_packet(state, &packet))
        {
            state->dispatch_errors++;
        }

        /* Preload any newly queued response bytes before the host polls again. */
        service_spi_tx_fifo(state);
        return true;
    }

    handle_receiver_status(state, status);
    return status != DEK_RECEIVER_FEED_STATUS_SYNCING;
}

static bool service_active_spi_transaction(dek_module_state_t *state)
{
    bool did_work = false;
    absolute_time_t deadline = make_timeout_time_us(DEK_SPI_INTERBYTE_TIMEOUT_US);

    if (!state->transaction_active)
    {
        begin_spi_transaction(state);
    }

    while (absolute_time_diff_us(get_absolute_time(), deadline) > 0)
    {
        if (spi_is_readable(DEK_SPI_PORT))
        {
            uint8_t byte = (uint8_t)spi_get_hw(DEK_SPI_PORT)->dr;

            state->spi_bytes_received++;
            state->spi_bytes_transmitted++;
            capture_spi_transaction_byte(state, byte);

            if (process_spi_rx_byte(state, byte))
            {
                did_work = true;
            }

            service_spi_tx_fifo(state);
            deadline = make_timeout_time_us(DEK_SPI_INTERBYTE_TIMEOUT_US);
            continue;
        }

        tight_loop_contents();
    }

    if (state->transaction_active)
    {
        finish_spi_transaction(state);
        did_work = true;
    }

    return did_work;
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
    stdio_init_all();
    sleep_ms(DEK_STARTUP_DELAY_MS);

    printf("DeK Reference Module starting\n");
    printf("Using external protocol library from external/DeK-Protocol\n");
    printf(
        "SPI slave pins: SCK=%u TX=%u RX=%u CS=%u\n",
        PICO_DEFAULT_SPI_SCK_PIN,
        PICO_DEFAULT_SPI_TX_PIN,
        PICO_DEFAULT_SPI_RX_PIN,
        PICO_DEFAULT_SPI_CSN_PIN);
    printf("USB serial monitor is the active debug output\n");

    {
        bool self_tests_ok = run_startup_self_tests();
        printf("Startup self-tests: %s\n", self_tests_ok ? "PASS" : "FAIL");
    }

    dek_module_state_t state;

    memset(&state, 0, sizeof(state));
    dek_transport_init(&state.transport);
    dek_packet_receiver_init(
        &state.receiver,
        state.receiver_buffer,
        sizeof(state.receiver_buffer));

    spi_link_init();
    service_spi_tx_fifo(&state);

    printf("SPI transport ready; idle byte is 0x%02X\n", DEK_SPI_IDLE_BYTE);

    while (true)
    {
        bool did_work = false;

        service_spi_tx_fifo(&state);

        if (spi_is_readable(DEK_SPI_PORT) || spi_cs_is_active())
        {
            if (service_active_spi_transaction(&state))
            {
                did_work = true;
            }
        }

        maybe_report_stats(&state);

        if (!did_work)
        {
            tight_loop_contents();
        }
    }
}
