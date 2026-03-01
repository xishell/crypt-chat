#ifndef UART_H
#define UART_H

/* Bit-banged UART: TX via GPIO, RX via 8x timer ISR. */

/* Standard baud rates */
#define UART_BAUD_9600 9600
#define UART_BAUD_19200 19200
#define UART_BAUD_115200 115200

/* UART Configuration */
typedef struct {
    int tx_pin;           /* GPIO pin for TX */
    int rx_pin;           /* GPIO pin for RX */
    unsigned int baud;    /* Baud rate (9600 recommended) */
    unsigned int bit_delay_us; /* Calculated bit delay in microseconds */
} uart_config_t;

/* ===== Basic UART Functions ===== */

/* Configure pins and timing */
void uart_init(uart_config_t *config);

/* Send one character (busy-wait) */
void uart_putc(uart_config_t *config, char c);

/* Send a string */
void uart_puts(uart_config_t *config, const char *str);

/* Poll for one character (-1 if none) */
int uart_getc(uart_config_t *config);

/* Flush RX */
void uart_flush_rx(uart_config_t *config);

/* ===== Interrupt-Based UART RX ===== */

#define UART_RX_BUFFER_SIZE 256

/* Init timer for 8x sampling */
void uart_rx_init_interrupt(uart_config_t *config);

/* Call from timer ISR */
void uart_rx_isr(void);

/* RX bytes available */
int uart_rx_available(void);

/* Pop one byte (-1 if empty) */
int uart_rx_getc(void);

/* Debug counters */
void uart_rx_debug_stats(void);

/* ===== Delays (busy-wait) ===== */

/* Microsecond delay via timer */
void delay_us(unsigned int us);

/* Millisecond delay */
void delay_ms(unsigned int ms);

/* ===== Accurate TX (timer) ===== */
void uart_tx_char(uart_config_t *config, char c);
void uart_tx_bytes(uart_config_t *config, const unsigned char *data, int len);

#endif /* UART_H */
