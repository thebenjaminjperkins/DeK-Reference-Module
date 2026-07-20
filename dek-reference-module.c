#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/uart.h"

#define UART_ID uart0
#define BAUD_RATE 115200

#define UART_TX_PIN 0
#define UART_RX_PIN 1

int main()
{
    stdio_init_all();

    sleep_ms(2000);

    printf("RP2350 Ready\n");

    uart_init(UART_ID, BAUD_RATE);

    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    while (true)
    {
        if (uart_is_readable(UART_ID))
        {
            char c = uart_getc(UART_ID);

            printf("%c", c);

            fflush(stdout);
        }
    }
}