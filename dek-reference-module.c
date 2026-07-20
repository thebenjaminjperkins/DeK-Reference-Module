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
    stdio_init_all();

    sleep_ms(2000);

    printf("RP2350 Ready\n");

    uart_init(UART_ID, BAUD_RATE);

    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    uint8_t rx_buffer[RX_BUFFER_SIZE];
    size_t rx_index = 0;

    dek_packet_receiver_t receiver;
    dek_packet_receiver_init(&receiver);

    dek_packet_t packet;

    while (true)
    {
        while (uart_is_readable(UART_ID))
        {
            uint8_t byte = uart_getc(UART_ID);

            if (dek_packet_receiver_feed(
                    &receiver,
                    byte,
                    &packet))
            {
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
