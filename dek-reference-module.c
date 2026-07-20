#include <stdio.h>
#include <stdint.h>

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "protocol/packet_receiver.h"

#define UART_ID uart0
#define BAUD_RATE 115200

#define UART_TX_PIN 0
#define UART_RX_PIN 1

#define RX_BUFFER_SIZE 512

int main(void)
{
    /*
     * Initialize USB stdio first so the firmware can emit startup and debug
     * messages to the host. The short delay gives a serial monitor time to
     * attach after reset, which is especially useful during bring-up.
     */
    stdio_init_all();

    sleep_ms(2000);

    printf("RP2350 Ready\n");

    /*
     * Configure UART0 as the raw transport input. Bytes received here are fed
     * into the packet receiver, which handles framing and validation for the
     * protocol layer.
     */
    uart_init(UART_ID, BAUD_RATE);

    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    /*
     * Keep a simple raw-byte scratch buffer available for future diagnostics or
     * hex-dump style logging while the protocol layer is still being brought
     * up. The active packet parsing path below uses `receiver` and `packet`.
     */
    uint8_t rx_buffer[RX_BUFFER_SIZE];
    size_t rx_index = 0;

    /* Maintain incremental parser state across UART reads. */
    dek_packet_receiver_t receiver;
    dek_packet_receiver_init(&receiver);

    /* Reused output structure populated whenever a complete packet is decoded. */
    dek_packet_t packet;

    while (true)
    {
        /*
         * Drain all currently available UART bytes each pass so packet assembly
         * keeps up with the incoming stream and avoids unnecessary latency.
         */
        while (uart_is_readable(UART_ID))
        {
            uint8_t byte = uart_getc(UART_ID);

            /*
             * The packet receiver consumes one byte at a time and only returns
             * true when a full packet has been assembled, length-checked, and
             * decoded successfully.
             */
            if (dek_packet_receiver_feed(
                    &receiver,
                    byte,
                    &packet))
            {
                /*
                 * For now, log the key header fields for visibility during
                 * bring-up. This makes it easy to confirm framing, sequence
                 * numbers, and payload sizing before higher-level handlers are
                 * added.
                 */
                printf("\nPacket received!\n");

                printf("Type: %u\n",
                    packet.header.message_type);

                printf("Sequence: %u\n",
                    packet.header.sequence_number);

                printf("Payload Length: %u\n",
                    packet.header.payload_length);
            }
        }
    }
}
