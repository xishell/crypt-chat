#ifndef UART_H
#define UART_H

/*
 * Software UART Implementation for DTEK-V
 * GPIO-based UART with interrupt-based RX
 */

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

/* Initialize UART with specified configuration */
void uart_init(uart_config_t *config);

/* Send single character (blocking) */
void uart_putc(uart_config_t *config, char c);

/* Send string (blocking) */
void uart_puts(uart_config_t *config, const char *str);

/* Receive single character (non-blocking, returns -1 if no data) */
int uart_getc(uart_config_t *config);

/* Clear any pending RX data */
void uart_flush_rx(uart_config_t *config);

/* ===== Interrupt-Based UART RX ===== */

#define UART_RX_BUFFER_SIZE 256

/* Initialize timer interrupt for RX sampling */
void uart_rx_init_interrupt(uart_config_t *config);

/* Timer ISR - call this from your timer interrupt handler */
void uart_rx_isr(void);

/* Check number of bytes available in RX buffer */
int uart_rx_available(void);

/* Get byte from RX buffer (non-blocking, returns -1 if empty) */
int uart_rx_getc(void);

/* Print debug statistics (for troubleshooting) */
void uart_rx_debug_stats(void);

/* ===== Chat Protocol ===== */

/* Sync pattern for reliable message framing */
#define CHAT_SYNC_BYTE 0xAA
#define CHAT_SYNC_COUNT 5
#define CHAT_MAX_MESSAGE 128

/* Send message with sync preamble for reliable delivery */
void chat_send_message(uart_config_t *uart, const char *message);

/* Receive message (waits for sync pattern, returns message length) */
int chat_receive_message(uart_config_t *uart, char *message, int max_len, unsigned int timeout_ms);

/* ===== Timing Helpers ===== */

/* Microsecond delay using hardware timer */
void delay_us(unsigned int us);

/* Millisecond delay */
void delay_ms(unsigned int ms);

#endif /* UART_H */
