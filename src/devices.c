#include "devices.h"

/* Memory-mapped I/O addresses */
#define LED_BASE 0x04000000
#define SW_BASE 0x04000010
#define BTN_BASE 0x040000D0
#define DISP_BASE 0x04000050
#define GPIO1_BASE 0x040000E0
#define GPIO2_BASE 0x040000F0

/* Display parameters */
#define NUM_DISPLAYS 6
#define DISP_STRIDE 0x10

/* 7-segment encoding (common cathode) */
static const unsigned char seg_table[16] = {
    0xC0, 0xF9, 0xA4, 0xB0, 0x99,      /* 0-4 */
    0x92, 0x82, 0xF8, 0x80, 0x90,      /* 5-9 */
    0x88, 0x83, 0xC6, 0xA1, 0x86, 0x8E /* A-F */
};

/* LED driver state */
static volatile unsigned int *led_ptr = (volatile unsigned int *)LED_BASE;
static unsigned int led_state = 0;

/* ===== LED Functions ===== */

void led_init(void) {
    led_state = 0;
    *led_ptr = 0;
}

void led_set(unsigned int mask) {
    led_state = mask & 0x3FF; /* 10 LEDs */
    *led_ptr = led_state;
}

void led_on(int led_num) {
    if (led_num >= 0 && led_num < 10) {
        led_state |= (1 << led_num);
        *led_ptr = led_state;
    }
}

void led_off(int led_num) {
    if (led_num >= 0 && led_num < 10) {
        led_state &= ~(1 << led_num);
        *led_ptr = led_state;
    }
}

void led_toggle(int led_num) {
    if (led_num >= 0 && led_num < 10) {
        led_state ^= (1 << led_num);
        *led_ptr = led_state;
    }
}

unsigned int led_get(void) { return led_state; }

/* ===== 7-Segment Display Functions ===== */

void display_init(void) { display_clear_all(); }

void display_set(int display_num, unsigned char value) {
    if (display_num < 0 || display_num >= NUM_DISPLAYS)
        return;

    volatile unsigned int *addr =
        (volatile unsigned int *)(DISP_BASE + (display_num * DISP_STRIDE));
    *addr = value & 0xFF;
}

void display_set_hex(int display_num, unsigned char digit) {
    if (digit < 16) {
        display_set(display_num, seg_table[digit]);
    }
}

void display_clear(int display_num) {
    display_set(display_num, 0xFF); /* All segments off */
}

void display_clear_all(void) {
    for (int i = 0; i < NUM_DISPLAYS; i++) {
        display_clear(i);
    }
}

void display_number(unsigned int number) {
    /* Display number in hexadecimal across all 6 displays */
    for (int i = 0; i < NUM_DISPLAYS; i++) {
        unsigned char digit = (number >> (i * 4)) & 0xF;
        display_set_hex(i, digit);
    }
}

void display_time(int hours, int minutes, int seconds) {
    /* Format: HH:MM:SS on 6 displays */
    display_set_hex(0, seconds % 10);
    display_set_hex(1, seconds / 10);
    display_set_hex(2, minutes % 10);
    display_set_hex(3, minutes / 10);
    display_set_hex(4, hours % 10);
    display_set_hex(5, hours / 10);
}

/* ===== Button Functions ===== */

void button_init(void) {
    /* Buttons are input-only, no initialization needed */
}

int button_is_pressed(void) {
    volatile unsigned int *btn_ptr = (volatile unsigned int *)BTN_BASE;
    return (*btn_ptr & 0x1) != 0;
}

int button_wait_press(void) {
    while (!button_is_pressed())
        ;
    return 1;
}

int button_wait_release(void) {
    while (button_is_pressed())
        ;
    return 1;
}

/* ===== Switch Functions ===== */

void switch_init(void) {
    /* Switches are input-only, no initialization needed */
}

unsigned int switch_read(void) {
    volatile unsigned int *sw_ptr = (volatile unsigned int *)SW_BASE;
    return *sw_ptr & 0x3FF; /* 10 switches */
}

int switch_get(int switch_num) {
    if (switch_num < 0 || switch_num >= 10)
        return 0;
    return (switch_read() >> switch_num) & 0x1;
}

/* ===== GPIO Functions ===== */

void gpio_init(void) {
    /* Set all GPIO pins as inputs by default */
    volatile unsigned int *gpio1_dir =
        (volatile unsigned int *)(GPIO1_BASE + 0x04);
    volatile unsigned int *gpio2_dir =
        (volatile unsigned int *)(GPIO2_BASE + 0x04);
    *gpio1_dir = 0x00000000; /* All inputs */
    *gpio2_dir = 0x00000000; /* All inputs */
}

void gpio_set_direction(int pin, int output) {
    if (pin < 0 || pin >= 40)
        return;

    volatile unsigned int *gpio_dir;
    int bit;

    if (pin < 20) {
        gpio_dir = (volatile unsigned int *)(GPIO1_BASE + 0x04);
        bit = pin;
    } else {
        gpio_dir = (volatile unsigned int *)(GPIO2_BASE + 0x04);
        bit = pin - 20;
    }

    if (output) {
        *gpio_dir |= (1 << bit); /* Set as output */
    } else {
        *gpio_dir &= ~(1 << bit); /* Set as input */
    }
}

void gpio_write(int pin, int value) {
    if (pin < 0 || pin >= 40)
        return;

    volatile unsigned int *gpio_data;
    int bit;

    if (pin < 20) {
        gpio_data = (volatile unsigned int *)GPIO1_BASE;
        bit = pin;
    } else {
        gpio_data = (volatile unsigned int *)GPIO2_BASE;
        bit = pin - 20;
    }

    if (value) {
        *gpio_data |= (1 << bit); /* Set high */
    } else {
        *gpio_data &= ~(1 << bit); /* Set low */
    }
}

int gpio_read(int pin) {
    if (pin < 0 || pin >= 40)
        return 0;

    volatile unsigned int *gpio_data;
    int bit;

    if (pin < 20) {
        gpio_data = (volatile unsigned int *)GPIO1_BASE;
        bit = pin;
    } else {
        gpio_data = (volatile unsigned int *)GPIO2_BASE;
        bit = pin - 20;
    }

    return (*gpio_data >> bit) & 0x1;
}
