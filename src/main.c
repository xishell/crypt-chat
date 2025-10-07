#include "devices.h"
#include "dtekv-lib.h"
#include "uart.h"

/* Global flag for send trigger */
volatile int send_flag = 0;

/* Input buffer for composing messages */
#define MAX_MESSAGE_LEN 128
char input_message[MAX_MESSAGE_LEN];
int input_msg_index = 0;

/* Button interrupt handler */
void my_button_handler(unsigned int button_state) {
    /* Button 1: Trigger send */
    if (button_state & (1 << 0)) {
        send_flag = 1;
    }
    /* Note: BTN2 is hardwired to board reset */
}

/* Timer ISR - handles UART RX */
void my_timer_isr(void) {
    uart_rx_isr();
}

/* Initialize all peripherals */
void init(void) {
    /* Initialize GPIO for UART */
    gpio_init();

    /* Initialize display */
    display_init();
    display_clear_all();

    /* Setup button interrupts */
    button_isr = my_button_handler;
    *BTN_IRQ_MASK = 0x0F;
    *BTN_EDGE_CAPTURE = 0x0F;

    /* Configure UART */
    uart_config_t esp_uart;
    esp_uart.tx_pin = 0;  /* GPIO 0 -> ESP8266 RX */
    esp_uart.rx_pin = 1;  /* GPIO 1 <- ESP8266 TX */
    esp_uart.baud = UART_BAUD_9600;
    uart_init(&esp_uart);

    /* Setup timer interrupt for UART RX */
    timer_isr = my_timer_isr;
    uart_rx_init_interrupt(&esp_uart);

    /* Enable interrupts globally */
    enable_interrupt();

    print("\n=== Encrypted Chat Application ===\n");
    print("System initialized\n");
    print("Press BTN1 to send message\n");
    print("Listening for incoming messages...\n\n");
}

/* Process received character from ESP8266 */
void process_rx_char(char c) {
    /* Echo received character */
    printc(c);
}

/* Send message triggered by button */
void send_message(uart_config_t *uart) {
    /* Read switch values as message */
    unsigned int switches = *SW_DATA;

    char message[32];
    message[0] = 'M';
    message[1] = 'S';
    message[2] = 'G';
    message[3] = ':';
    message[4] = ' ';

    /* Convert switch value to hex string */
    int idx = 5;
    for (int i = 2; i >= 0; i--) {
        unsigned int nibble = (switches >> (i * 4)) & 0xF;
        if (nibble < 10) {
            message[idx++] = '0' + nibble;
        } else {
            message[idx++] = 'A' + (nibble - 10);
        }
    }
    message[idx++] = '\r';
    message[idx++] = '\n';
    message[idx] = '\0';

    /* TODO: Encrypt message */

    print("\n[Sending: ");
    print(message);
    print("]\n");

    /* Send via UART */
    chat_send_message(uart, message);

    /* Visual feedback */
    led_toggle(0);
}

int main(void) {
    uart_config_t esp_uart;

    /* Initialize system */
    init();

    /* Store UART config for main loop */
    esp_uart.tx_pin = 0;
    esp_uart.rx_pin = 1;
    esp_uart.baud = UART_BAUD_9600;

    /* Main loop */
    while (1) {
        /* Check for received data (non-blocking) */
        if (uart_rx_available() > 0) {
            int c = uart_rx_getc();
            if (c != -1) {
                process_rx_char((char)c);
            }
        }

        /* Check if send button pressed */
        if (send_flag) {
            send_flag = 0;
            send_message(&esp_uart);
        }

        /* Small delay to reduce CPU usage */
        /* Note: Using busy-wait to avoid interfering with timer interrupt */
        for (volatile int i = 0; i < 1000; i++);
    }

    return 0;
}
