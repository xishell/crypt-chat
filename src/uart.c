/* Bit-banged UART: TX GPIO, RX 8x ISR. */
#include "uart.h"
#include "devices.h"
#include "dtekv-lib.h"

/* 30 MHz clock */
#define CPU_FREQ_HZ 30000000

/* Fine-tune if needed; 9600 baud ~104 µs/bit */
#define BIT_DELAY_ADJUST_US 0

/* Interval timer bits */
#define TIMER_START 0x04 /* START bit */
#define TIMER_CONT 0x02  /* CONT bit */
#define TIMER_ITO 0x01   /* Timeout bit in status */

/* ===== Timing helpers ===== */

/* One-shot delay in CPU cycles */
void timer_delay_cycles(unsigned int cycles) {
    /* Stop timer */
    *TIMER_CONTROL = 0x08; /* STOP bit */

    /* Set period */
    *TIMER_PERIODL = cycles & 0xFFFF;
    *TIMER_PERIODH = (cycles >> 16) & 0xFFFF;

    /* Start timer (one-shot mode) */
    *TIMER_CONTROL = TIMER_START;

    /* Wait for timeout */
    while ((*TIMER_STATUS & TIMER_ITO) == 0) {
        /* Wait for timer to reach 0 */
    }

    /* Clear flag */
    *TIMER_STATUS = 0;
}

/* Microsecond delay */
void delay_us(unsigned int us) {
    timer_delay_cycles(us * (CPU_FREQ_HZ / 1000000));
}

/* Millisecond delay */
void delay_ms(unsigned int ms) {
    for (unsigned int i = 0; i < ms; i++) {
        delay_us(1000);
    }
}

/* ===== Basic UART ===== */

void uart_init(uart_config_t *config) {
    /* TX output, RX input */
    gpio_set_direction(config->tx_pin, 1); /* TX is output */
    gpio_set_direction(config->rx_pin, 0); /* RX is input */

    /* Idle high */
    gpio_write(config->tx_pin, 1);

    /* Precompute timing */
    config->bit_delay_us = (1000000 / config->baud) + BIT_DELAY_ADJUST_US;

    /* Short settle */
    delay_ms(10);
}

void uart_putc(uart_config_t *config, char c) {
    /* Use simple delay loop */
    unsigned int bit_delay_cycles = config->bit_delay_us * (CPU_FREQ_HZ / 1000000);

    /* Start bit */
    gpio_write(config->tx_pin, 0);
    delay(bit_delay_cycles);

    /* 8 data bits */
    for (int i = 0; i < 8; i++) {
        gpio_write(config->tx_pin, (c >> i) & 0x1);
        delay(bit_delay_cycles);
    }

    /* Stop bit */
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

/* ===== RX via timer ISR ===== */

/* RX state machine */
static volatile int rx_pin_number = 1;
static volatile int rx_state = 0;  /* 0=idle, 1=start, 2=data bits, 3=stop */
static volatile int rx_sample_count = 0;
static volatile unsigned char rx_shift_reg = 0;
static volatile int rx_bit_count = 0;

/* Debug counters */
static volatile unsigned int rx_isr_count = 0;
static volatile unsigned int rx_bytes_received = 0;

/*
 * RX ring buffer — single-producer (ISR writes head) / single-consumer
 * (main loop reads tail).  Safe without locks on single-core bare-metal
 * RISC-V because each index is only written by one side and volatile
 * prevents reordering.
 */
static volatile unsigned char rx_buffer[UART_RX_BUFFER_SIZE];
static volatile int rx_buffer_head = 0;
static volatile int rx_buffer_tail = 0;

/* Init timer for 8× baud sampling */
void uart_rx_init_interrupt(uart_config_t *config) {
    rx_pin_number = config->rx_pin;

    /* 8× oversampling */
    unsigned int sample_rate = config->baud * 8;
    unsigned int timer_period = CPU_FREQ_HZ / sample_rate;

    /* Stop timer */
    *TIMER_CONTROL = 0x08;

    /* Set period */
    *TIMER_PERIODL = timer_period & 0xFFFF;
    *TIMER_PERIODH = (timer_period >> 16) & 0xFFFF;

    /* CONT + ITO */
    *TIMER_CONTROL = 0x07;  /* ITO | CONT | START */

    /* Reset state */
    rx_state = 0;
    rx_sample_count = 0;
    rx_buffer_head = 0;
    rx_buffer_tail = 0;
}

/* Timer ISR: sample + advance state */
void uart_rx_isr(void) {
    /* Clear timer interrupt flag */
    *TIMER_STATUS = 0;

    rx_isr_count++;

    int rx_bit = gpio_read(rx_pin_number);

    switch (rx_state) {
        case 0:  /* idle */
            if (rx_bit == 0) {
                rx_state = 1;
                rx_sample_count = 0;
            }
            break;

        case 1:  /* mid-start check */
            rx_sample_count++;
            if (rx_sample_count == 4) {  /* Middle of start bit */
                if (rx_bit == 0) {
                    /* valid start */
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
                    /* move to stop */
                    rx_state = 3;
                }
            }
            break;

        case 3:  /* stop */
            rx_sample_count++;
            if (rx_sample_count >= 4) {
                /* push byte */
                int next_head = (rx_buffer_head + 1) % UART_RX_BUFFER_SIZE;
                if (next_head != rx_buffer_tail) {
                    rx_buffer[rx_buffer_head] = rx_shift_reg;
                    rx_buffer_head = next_head;
                    rx_bytes_received++;
                }

                /* idle */
                rx_state = 0;
                rx_sample_count = 0;
            }
            break;
    }
}

/* RX count */
int uart_rx_available(void) {
    return (rx_buffer_head - rx_buffer_tail + UART_RX_BUFFER_SIZE) % UART_RX_BUFFER_SIZE;
}

/* Pop next byte */
int uart_rx_getc(void) {
    if (rx_buffer_head == rx_buffer_tail) {
        return -1;
    }

    unsigned char c = rx_buffer[rx_buffer_tail];
    rx_buffer_tail = (rx_buffer_tail + 1) % UART_RX_BUFFER_SIZE;
    return c;
}

/* RX stats */
void uart_rx_debug_stats(void) {
    print("ISR calls: ");
    print_udec(rx_isr_count);
    print("\nBytes received: ");
    print_udec(rx_bytes_received);
    print("\nCurrent state: ");
    print_dec(rx_state);
    print("\n");
}

/* ===== Accurate TX (busy-wait) ===== */

/* Busy-wait delay in CPU cycles without using the hardware timer */
static void busywait_cycles(unsigned int cycles) {
    for (volatile unsigned int i = 0; i < cycles; i++) ;
}

/* Send one character using busy-wait delays to avoid conflicting with the RX timer */
void uart_tx_char(uart_config_t *config, char c) {
    unsigned int bit_delay_cycles = CPU_FREQ_HZ / config->baud;
    gpio_write(config->tx_pin, 0); /* start */
    busywait_cycles(bit_delay_cycles);
    for (int i = 0; i < 8; i++) {
        gpio_write(config->tx_pin, (c >> i) & 0x1);
        busywait_cycles(bit_delay_cycles);
    }
    gpio_write(config->tx_pin, 1); /* stop */
    busywait_cycles(bit_delay_cycles);
}

/* Send bytes (precise timing) */
void uart_tx_bytes(uart_config_t *config, const unsigned char *data, int len) {
    for (int i = 0; i < len; i++) {
        uart_tx_char(config, (char)data[i]);
    }
}
