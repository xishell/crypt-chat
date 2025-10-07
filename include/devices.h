#ifndef DEVICES_H
#define DEVICES_H

/*
 * DTEK-V Device Drivers
 * High-level hardware abstraction layer
 */

/* ===== Hardware Register Definitions ===== */

/* Memory-mapped I/O base addresses */
#define TIMER_BASE 0x04000020
#define SWITCHES_BASE 0x04000010
#define BUTTON_BASE 0x040000D0
#define JTAG_UART_BASE 0x04000040

/* Timer registers */
#define TIMER_STATUS ((volatile unsigned short *)(TIMER_BASE + 0x00))
#define TIMER_CONTROL ((volatile unsigned short *)(TIMER_BASE + 0x04))
#define TIMER_PERIODL ((volatile unsigned short *)(TIMER_BASE + 0x08))
#define TIMER_PERIODH ((volatile unsigned short *)(TIMER_BASE + 0x0C))
#define TIMER_SNAPL ((volatile unsigned short *)(TIMER_BASE + 0x10))
#define TIMER_SNAPH ((volatile unsigned short *)(TIMER_BASE + 0x14))

/* Switch registers */
#define SW_DATA ((volatile unsigned int *)(SWITCHES_BASE + 0x00))
#define SW_DIRECTION ((volatile unsigned int *)(SWITCHES_BASE + 0x04))
#define SW_IRQ_MASK ((volatile unsigned int *)(SWITCHES_BASE + 0x08))
#define SW_EDGE_CAPTURE ((volatile unsigned int *)(SWITCHES_BASE + 0x0C))

/* Button registers */
#define BTN_DATA ((volatile unsigned int *)(BUTTON_BASE + 0x00))
#define BTN_DIRECTION ((volatile unsigned int *)(BUTTON_BASE + 0x04))
#define BTN_IRQ_MASK ((volatile unsigned int *)(BUTTON_BASE + 0x08))
#define BTN_EDGE_CAPTURE ((volatile unsigned int *)(BUTTON_BASE + 0x0C))

/* JTAG UART registers */
#define JTAG_UART_DATA ((volatile unsigned int *)(JTAG_UART_BASE + 0x00))
#define JTAG_UART_CTRL ((volatile unsigned int *)(JTAG_UART_BASE + 0x04))

/* JTAG UART control register bits */
#define JTAG_UART_WSPACE_MASK 0xFFFF0000 /* Write space available */
#define JTAG_UART_RVALID_MASK 0x00008000 /* Read valid bit */
#define JTAG_UART_DATA_MASK 0x000000FF   /* Data byte mask */

/* Interrupt source definitions */
#define IRQ_TIMER 16
#define IRQ_SWITCHES 17
#define IRQ_BUTTON 18

/* ===== High-Level Device Drivers ===== */

/* LED Functions */
void led_init(void);
void led_set(unsigned int mask);
void led_on(int led_num);
void led_off(int led_num);
void led_toggle(int led_num);
unsigned int led_get(void);

/* 7-Segment Display Functions */
void display_init(void);
void display_clear(int display_num);
void display_clear_all(void);
void display_hex(unsigned int number);      /* Display as hexadecimal */
void display_decimal(unsigned int number);  /* Display as decimal */
void display_digit(int display_num, unsigned char digit); /* Show 0-F on one display */
void display_string(const char *str);       /* Display up to 6 characters */

/* Button Functions */
void button_init(void);
int button_is_pressed(void);

/* Switch Functions */
void switch_init(void);
unsigned int switch_read(void);
int switch_get(int switch_num);

/* GPIO Functions */
#define GPIO_PIN_COUNT 40

void gpio_init(void);
void gpio_set_direction(int pin, int output); /* 1=output, 0=input */
void gpio_write(int pin, int value);
int gpio_read(int pin);
void gpio_toggle(int pin);

#endif /* DEVICES_H */
