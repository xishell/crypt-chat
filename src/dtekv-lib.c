#include "dtekv-lib.h"
#include "devices.h"

/* ===== ISR Function Pointers ===== */

void (*timer_isr)(void) = 0;
void (*switch_isr)(unsigned int) = 0;
void (*button_isr)(unsigned int) = 0;

/* ===== JTAG UART I/O Functions ===== */

/* Print a single character to JTAG UART */
void printc(char c) {
    while (((*JTAG_UART_CTRL) & JTAG_UART_WSPACE_MASK) == 0)
        ;
    *JTAG_UART_DATA = c;
}

/* Print a null-terminated string */
void print(char *s) {
    if (s == 0)
        return;
    while (*s != '\0') {
        printc(*s);
        s++;
    }
}

/* Read a single character from JTAG UART (non-blocking) */
char readc(void) {
    unsigned int data = *JTAG_UART_DATA;
    if (data & JTAG_UART_RVALID_MASK) {
        return (char)(data & JTAG_UART_DATA_MASK);
    }
    return 0; /* No data available */
}

/* Check if data is available to read */
int read_available(void) {
    return (*JTAG_UART_DATA & JTAG_UART_RVALID_MASK) != 0;
}

/* ===== Number Formatting Functions ===== */

/* Print a signed decimal integer */
void print_dec(int x) {
    if (x < 0) {
        printc('-');
        x = -x;
    }
    print_udec((unsigned int)x);
}

/* Print an unsigned decimal integer */
void print_udec(unsigned int x) {
    unsigned int divisor = 1000000000;
    char started = 0;

    do {
        int digit = x / divisor;
        if (digit != 0 || started) {
            printc('0' + digit);
            started = 1;
        }
        x -= digit * divisor;
        divisor /= 10;
    } while (divisor != 0);

    if (!started) {
        printc('0');
    }
}

/* Print a 32-bit hexadecimal value (8 digits) */
void print_hex32(unsigned int x) {
    print_hex(x, 8);
}

/* Print a hexadecimal value with specified number of digits */
void print_hex(unsigned int x, int digits) {
    printc('0');
    printc('x');
    for (int i = digits - 1; i >= 0; i--) {
        char nibble = (char)((x >> (i * 4)) & 0xF);
        if (nibble < 10) {
            printc('0' + nibble);
        } else {
            printc('A' + (nibble - 10));
        }
    }
}

/* Print a binary value with specified number of bits */
void print_bin(unsigned int x, int bits) {
    printc('0');
    printc('b');
    for (int i = bits - 1; i >= 0; i--) {
        printc((x & (1 << i)) ? '1' : '0');
    }
}

/* ===== Exception Handler ===== */

void handle_exception(unsigned arg0, unsigned arg1, unsigned arg2,
                      unsigned arg3, unsigned arg4, unsigned arg5,
                      unsigned mcause, unsigned syscall_num) {
    switch (mcause) {
    case 0:
        print("\n[EXCEPTION] Instruction address misalignment.\n");
        break;
    case 2:
        print("\n[EXCEPTION] Illegal instruction.\n");
        break;
    case 11: /* Environment call (ecall) */
        if (syscall_num == 4) {
            print((char *)arg0);
        } else if (syscall_num == 11) {
            printc(arg0);
        }
        return;
    default:
        print("\n[EXCEPTION] Unknown error (mcause=");
        print_udec(mcause);
        print(").\n");
        break;
    }

    print("Exception Address: ");
    print_hex32(arg0);
    printc('\n');
    while (1)
        ; /* Halt on exception */
}

/* ===== Interrupt Handler ===== */

void handle_interrupt(unsigned cause) {
    switch (cause) {
    case IRQ_TIMER:
        /* Timer interrupt - fires when timer reaches 0 */
        /* Clear timeout flag by writing to status register */
        *TIMER_STATUS = 0;

        /* Call user-defined timer ISR if provided */
        if (timer_isr) {
            timer_isr();
        } else {
            print("[IRQ] Timer interrupt (no handler)\n");
        }
        break;

    case IRQ_SWITCHES:
        /* Switch interrupt - fires when switch state changes */
        /* Read switch state and edge capture */
        unsigned int switch_state = *SW_DATA;
        unsigned int edge_bits = *SW_EDGE_CAPTURE;

        /* Clear edge capture by writing 1s to the bits that fired */
        *SW_EDGE_CAPTURE = edge_bits;

        /* Call user-defined switch ISR if provided */
        if (switch_isr) {
            switch_isr(switch_state);
        } else {
            print("[IRQ] Switch interrupt, state: ");
            print_hex(switch_state, 4);
            print(" edges: ");
            print_hex(edge_bits, 4);
            print("\n");
        }
        break;

    case IRQ_BUTTON:
        /* Button interrupt - fires when button is pressed */
        /* Read button state and edge capture */
        unsigned int button_state = *BTN_DATA;
        unsigned int btn_edge = *BTN_EDGE_CAPTURE;

        /* Clear edge capture by writing 1s to the bits that fired */
        *BTN_EDGE_CAPTURE = btn_edge;

        /* Call user-defined button ISR if provided */
        if (button_isr) {
            button_isr(button_state);
        } else {
            print("[IRQ] Button interrupt, state: ");
            print_hex(button_state, 2);
            print("\n");
        }
        break;

    default:
        /* Unknown interrupt */
        print("[IRQ] Unknown interrupt (cause=");
        print_udec(cause);
        print(")\n");
        break;
    }
}

/* ===== Utility Functions ===== */

/* Simple delay loop */
void delay(unsigned int cycles) {
    for (unsigned int i = 0; i < cycles; i++) {
        /* Volatile to prevent optimization */
        volatile int dummy = i;
        (void)dummy;
    }
}
