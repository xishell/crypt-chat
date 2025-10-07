#include "uart.h"
#include "devices.h"
#include "dtekv-lib.h"

/* Timing constants - CPU documented at 30MHz */
#define CPU_FREQ_HZ 30000000

/* Fine-tuning: adjust bit delay in microseconds
 * At 9600 baud, bit period should be ~104 us */
#define BIT_DELAY_ADJUST_US 0

/* Timer control bits (Altera Interval Timer) */
#define TIMER_START 0x04 /* START bit */
#define TIMER_CONT 0x02  /* CONT bit */
#define TIMER_ITO 0x01   /* Timeout bit in status */

/* ===== Timing Functions ===== */

/* Hardware timer-based precise delay (cycle-accurate) */
void timer_delay_cycles(unsigned int cycles) {
    /* Stop timer */
    *TIMER_CONTROL = 0x08; /* STOP bit */

    /* Set period (timer counts DOWN from this value) */
    *TIMER_PERIODL = cycles & 0xFFFF;
    *TIMER_PERIODH = (cycles >> 16) & 0xFFFF;

    /* Start timer (one-shot mode) */
    *TIMER_CONTROL = TIMER_START;

    /* Poll for timeout */
    while ((*TIMER_STATUS & TIMER_ITO) == 0) {
        /* Wait for timer to reach 0 */
    }

    /* Clear timeout flag */
    *TIMER_STATUS = 0;
}

/* Microsecond delay using hardware timer */
void delay_us(unsigned int us) {
    timer_delay_cycles(us * (CPU_FREQ_HZ / 1000000));
}

/* Millisecond delay */
void delay_ms(unsigned int ms) {
    for (unsigned int i = 0; i < ms; i++) {
        delay_us(1000);
    }
}

/* ===== Basic UART Functions ===== */

void uart_init(uart_config_t *config) {
    /* Configure GPIO pins */
    gpio_set_direction(config->tx_pin, 1); /* TX is output */
    gpio_set_direction(config->rx_pin, 0); /* RX is input */

    /* Set TX idle state (high) */
    gpio_write(config->tx_pin, 1);

    /* Calculate bit delay in microseconds with fine-tuning */
    config->bit_delay_us = (1000000 / config->baud) + BIT_DELAY_ADJUST_US;

    /* Small delay to stabilize */
    delay_ms(10);
}

void uart_putc(uart_config_t *config, char c) {
    /* Use dtekv-lib delay() which doesn't reconfigure timer */
    unsigned int bit_delay_cycles = config->bit_delay_us * (CPU_FREQ_HZ / 1000000);

    /* Start bit (low) */
    gpio_write(config->tx_pin, 0);
    delay(bit_delay_cycles);

    /* Data bits (LSB first) */
    for (int i = 0; i < 8; i++) {
        gpio_write(config->tx_pin, (c >> i) & 0x1);
        delay(bit_delay_cycles);
    }

    /* Stop bit (high) */
    gpio_write(config->tx_pin, 1);
    delay(bit_delay_cycles);
}

void uart_puts(uart_config_t *config, const char *str) {
    while (*str) {
        uart_putc(config, *str++);
    }
}

int uart_getc(uart_config_t *config) {
    unsigned int bit_delay = config->bit_delay_us;

    /* Check for start bit */
    if (gpio_read(config->rx_pin) != 0) {
        return -1; /* No start bit */
    }

    /* Wait to middle of start bit */
    delay_us(bit_delay / 2);

    /* Verify start bit is still low */
    if (gpio_read(config->rx_pin) != 0) {
        return -1; /* False start */
    }

    /* Wait to middle of first data bit */
    delay_us(bit_delay);

    /* Read data bits (LSB first) */
    char result = 0;
    for (int i = 0; i < 8; i++) {
        if (gpio_read(config->rx_pin)) {
            result |= (1 << i);
        }
        delay_us(bit_delay);
    }

    /* Wait for stop bit to finish */
    delay_us(bit_delay / 2);

    return (int)result;
}

void uart_flush_rx(uart_config_t *config) {
    /* Read and discard any pending data */
    int attempts = 0;
    while (attempts < 100) {
        if (uart_getc(config) == -1) {
            break;
        }
        attempts++;
    }
}

/* ===== Interrupt-Based UART RX ===== */

/* RX state machine */
static volatile int rx_pin_number = 1;
static volatile int rx_state = 0;  /* 0=idle, 1=start, 2=data bits, 3=stop */
static volatile int rx_sample_count = 0;
static volatile unsigned char rx_shift_reg = 0;
static volatile int rx_bit_count = 0;

/* Debug counters */
static volatile unsigned int rx_isr_count = 0;
static volatile unsigned int rx_bytes_received = 0;

/* Circular buffer for received bytes */
static volatile unsigned char rx_buffer[UART_RX_BUFFER_SIZE];
static volatile int rx_buffer_head = 0;
static volatile int rx_buffer_tail = 0;

/* Initialize timer interrupt for UART RX at 8x baud rate */
void uart_rx_init_interrupt(uart_config_t *config) {
    rx_pin_number = config->rx_pin;

    /* Calculate timer period for 8x oversampling
     * For 9600 baud: need 76800 Hz sampling
     * Period = CPU_FREQ / sample_rate */
    unsigned int sample_rate = config->baud * 8;
    unsigned int timer_period = CPU_FREQ_HZ / sample_rate;

    /* Stop timer */
    *TIMER_CONTROL = 0x08;

    /* Set period */
    *TIMER_PERIODL = timer_period & 0xFFFF;
    *TIMER_PERIODH = (timer_period >> 16) & 0xFFFF;

    /* Start timer in continuous mode with interrupt */
    *TIMER_CONTROL = 0x07;  /* ITO | CONT | START */

    /* Reset state */
    rx_state = 0;
    rx_sample_count = 0;
    rx_buffer_head = 0;
    rx_buffer_tail = 0;
}

/* Timer ISR - called at 8x baud rate */
void uart_rx_isr(void) {
    /* Clear timer interrupt flag */
    *TIMER_STATUS = 0;

    rx_isr_count++;

    int rx_bit = gpio_read(rx_pin_number);

    switch (rx_state) {
        case 0:  /* IDLE - waiting for start bit */
            if (rx_bit == 0) {
                rx_state = 1;
                rx_sample_count = 0;
            }
            break;

        case 1:  /* START BIT - verify it's real */
            rx_sample_count++;
            if (rx_sample_count == 4) {  /* Middle of start bit */
                if (rx_bit == 0) {
                    /* Valid start bit */
                    rx_state = 2;
                    rx_sample_count = 0;
                    rx_shift_reg = 0;
                    rx_bit_count = 0;
                } else {
                    /* False start */
                    rx_state = 0;
                }
            } else if (rx_sample_count >= 8) {
                rx_state = 2;
                rx_sample_count = 0;
                rx_shift_reg = 0;
                rx_bit_count = 0;
            }
            break;

        case 2:  /* DATA BITS (8 bits) */
            rx_sample_count++;
            if (rx_sample_count == 4) {  /* Middle sample */
                /* Shift in bit (LSB first) */
                if (rx_bit) {
                    rx_shift_reg |= (1 << rx_bit_count);
                }
                rx_bit_count++;
            } else if (rx_sample_count >= 8) {
                rx_sample_count = 0;

                if (rx_bit_count >= 8) {
                    /* All 8 bits received, move to stop bit */
                    rx_state = 3;
                }
            }
            break;

        case 3:  /* STOP BIT */
            rx_sample_count++;
            if (rx_sample_count >= 4) {
                /* Store byte in buffer */
                int next_head = (rx_buffer_head + 1) % UART_RX_BUFFER_SIZE;
                if (next_head != rx_buffer_tail) {
                    rx_buffer[rx_buffer_head] = rx_shift_reg;
                    rx_buffer_head = next_head;
                    rx_bytes_received++;
                }

                /* Back to idle */
                rx_state = 0;
                rx_sample_count = 0;
            }
            break;
    }
}

/* Check how many bytes are available */
int uart_rx_available(void) {
    return (rx_buffer_head - rx_buffer_tail + UART_RX_BUFFER_SIZE) % UART_RX_BUFFER_SIZE;
}

/* Get byte from buffer (non-blocking) */
int uart_rx_getc(void) {
    if (rx_buffer_head == rx_buffer_tail) {
        return -1;
    }

    unsigned char c = rx_buffer[rx_buffer_tail];
    rx_buffer_tail = (rx_buffer_tail + 1) % UART_RX_BUFFER_SIZE;
    return c;
}

/* Debug statistics */
void uart_rx_debug_stats(void) {
    print("ISR calls: ");
    print_udec(rx_isr_count);
    print("\nBytes received: ");
    print_udec(rx_bytes_received);
    print("\nCurrent state: ");
    print_dec(rx_state);
    print("\n");
}

/* ===== Chat Protocol Functions ===== */

/* Send message with sync preamble */
void chat_send_message(uart_config_t *uart, const char *message) {
    /* Send sync bytes for receiver to lock onto */
    for (int i = 0; i < CHAT_SYNC_COUNT; i++) {
        uart_putc(uart, CHAT_SYNC_BYTE);
        delay_ms(2);
    }

    /* Send start marker */
    uart_putc(uart, 0xFF);
    delay_ms(2);

    /* Send message */
    while (*message) {
        uart_putc(uart, *message);
        delay_ms(2);
        message++;
    }

    /* Send end marker */
    uart_putc(uart, 0x00);
    delay_ms(2);
}

/* Receive message - waits for sync pattern */
int chat_receive_message(uart_config_t *uart, char *message, int max_len, unsigned int timeout_ms) {
    int sync_count = 0;
    int idx = 0;
    unsigned int elapsed = 0;
    int in_message = 0;

    while (elapsed < timeout_ms && idx < max_len - 1) {
        int c = uart_getc(uart);

        if (c != -1) {
            if (!in_message) {
                /* Looking for sync pattern */
                if (c == CHAT_SYNC_BYTE) {
                    sync_count++;
                } else if (c == 0xFF && sync_count >= 3) {
                    /* Found start marker */
                    in_message = 1;
                    sync_count = 0;
                } else {
                    sync_count = 0;
                }
            } else {
                /* In message - collect characters */
                if (c == 0x00) {
                    /* End marker */
                    message[idx] = '\0';
                    return idx;
                }

                message[idx++] = (char)c;
            }
        }

        delay_us(100);
        elapsed++;
    }

    message[idx] = '\0';
    return idx;
}
